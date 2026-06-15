# 视频播放功能说明

本项目的视频播放采用“PC 端预转码，MCU 端流式刷屏”的方案。MCU 不解码 MP4/JPG，也不把整段视频放进 SRAM；视频先转换为 `.v565` 文件，上传到 W25Q32 的 LittleFS 分区，播放时逐行读取并写入 ST7789。

## 格式与压缩

`.v565` 文件头固定为 24 字节，包含 `V565` magic、版本、标志位、宽高、FPS、帧数、单帧 RGB565 展开大小、压缩数据大小。

当前支持 6 种编码：

- `mono1-rle`：默认推荐给黑白视频。1-bit 黑白帧 + 行级 RLE，最省空间。
- `mono1`：1-bit 黑白帧，不做 RLE。
- `index8-rle`：推荐给彩色视频。全局 256 色调色板 + 8-bit 索引帧 + 行级 RLE。
- `index8`：全局 256 色调色板 + 8-bit 索引帧，不做 RLE，体积约为 RGB565 的一半。
- `rle`：RGB565 行级 RLE。适合像素风或大面积纯色画面，照片类视频可能变大。
- `none`：旧版裸 RGB565 连续帧，便于排查问题。

`mono1-rle` 会额外写入 4 字节黑白调色板和每帧 4 字节偏移表。`index8-rle` 会额外写入 512 字节调色板和每帧 4 字节偏移表。MCU 端只用一行缓冲和一张调色板，不需要整帧缓存。

## 容量估算

- 240x320 RGB565 裸帧单帧约 153,600 字节。
- 240x320 `index8` 单帧约 76,800 字节，再叠加少量头部/调色板开销。
- 240x320 `mono1` 单帧约 9,600 字节，再叠加少量头部/调色板开销。
- `index8-rle` 对动画、UI、像素风通常更小；对噪声很强的真实视频可能接近 `index8`。
- `mono1-rle` 对 Bad Apple 这类黑白高对比视频压缩率最好。
- 建议先用 160x120、8 fps、40 帧左右测试。

## 电脑端依赖

```powershell
pip install pyserial pillow imageio imageio-ffmpeg
```

GIF、PNG/JPG 帧目录只需要 Pillow；MP4 等视频文件通常需要 `imageio` 和 `imageio-ffmpeg`。

## 只生成 V565 文件

默认使用 `mono1-rle`：

```powershell
python scripts/flash_manager.py pack-video input.mp4 build/demo.v565 --width 160 --height 120 --fps 8 --fit contain --max-frames 40
```

可以用 0 基帧号截取片段，`--end-frame` 为包含式结束帧：

```powershell
python scripts/flash_manager.py pack-video input.mp4 build/part.v565 --width 160 --height 120 --fps 8 --start-frame 240 --end-frame 1200
```

`--start-frame` 和 `--end-frame` 指源视频帧号。对于 MP4 等带 FPS
元数据的视频，工具会按源 FPS 自动抽帧到 `--fps`，例如 30 FPS
源视频指定 `--fps 8` 时只保留时间轴上需要的帧，不会把全部源帧按
8 FPS 慢放。帧目录没有源 FPS 元数据，因此仍按一张图片对应一帧处理。

如果要对比体积，可以指定压缩模式：

```powershell
python scripts/flash_manager.py pack-video input.mp4 build/demo_raw.v565 --width 160 --height 120 --fps 8 --fit contain --max-frames 40 --compression none
python scripts/flash_manager.py pack-video input.mp4 build/demo_mono.v565 --width 160 --height 120 --fps 8 --fit contain --max-frames 40 --compression mono1-rle
python scripts/flash_manager.py pack-video input.mp4 build/demo_i8.v565 --width 160 --height 120 --fps 8 --fit contain --max-frames 40 --compression index8
python scripts/flash_manager.py pack-video input.mp4 build/demo_i8rle.v565 --width 160 --height 120 --fps 8 --fit contain --max-frames 40 --compression index8-rle
```

也可以把一组帧图片放到目录中：

```powershell
python scripts/flash_manager.py pack-video frames build/demo.v565 --width 160 --height 120 --fps 8 --fit cover --max-frames 40
```

## 上传视频

上传固件需要启用 Flash Manager。临时修改 `config/app_config.h`：

```c
#define LOW_KNIGHT_STANDALONE_ENABLE 0
#define VIDEO_PLAYER_ENABLE 0
#define FLASH_MGR_ENABLE 1
#define GAME_CONSOLE_ENABLE 0
```

编译、烧录并复位后上传：

```powershell
python scripts/flash_manager.py COM6 upload-video input.mp4 /demo.v565 --width 160 --height 120 --fps 8 --fit contain --max-frames 40
python scripts/flash_manager.py COM6 info /demo.v565
```

当前 Flash Manager 默认使用 921600 bps；如果你临时烧的是旧固件，才需要额外加 `--baud 115200`。

上传指定片段：

```powershell
python scripts/flash_manager.py COM6 upload-video input.mp4 /demo.v565 --width 160 --height 120 --fps 8 --start-frame 240 --end-frame 1200
```

## 播放视频

上传完成后，修改 `config/app_config.h`：

```c
#define VIDEO_PLAYER_ENABLE 1
#define VIDEO_PLAYER_LOOP 1
#define VIDEO_PLAYER_PATH "/demo.v565"
#define FLASH_MGR_ENABLE 0
```

重新编译烧录。启动后会进入视频播放器：

- 如果找到 `/demo.v565`，居中播放。
- 按 SW 按键暂停/继续。
- 如果文件缺失或格式不匹配，会显示 `VIDEO MISS`。

## 后续可优化点

- 增加多行批量读取，减少 LittleFS seek/read 次数。
- 增加帧差编码，进一步压缩连续帧变化小的视频。
- 接入蜂鸣器或外部音频模块实现音频同步。
