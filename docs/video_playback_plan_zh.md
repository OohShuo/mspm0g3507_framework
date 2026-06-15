# feat-video 视频播放与开机动画实现方案

本文档用于把“可替换视频播放”和“开机动画”任务交给另一个 Agent 实施。目标不是在 MCU 上解码 MP4/GIF，而是在 PC 端预处理为 MCU 可顺序读取的帧流，运行时只做按行读取和 RGB565 写屏。

## 目标

1. 在 `feat-video` 分支实现一个独立视频播放模块。
2. 视频文件可通过 UART 上传到 W25Q32 的 LittleFS 分区，替换文件后固件不需要重新编译。
3. 第一阶段支持全屏无音频开机动画，失败时自动跳过，不影响后续应用启动。
4. 第二阶段可把同一播放器注册成游戏/演示 app，方便手动测试不同视频。

非目标：

- 不在 MCU 上解码 MP4、GIF、PNG、JPEG。
- 不追求 24/30 FPS 全色视频；先以 240x320、RGB565、5-12 FPS 为现实目标。
- 不把整帧或多帧加载进 SRAM。SRAM 已经很紧，只允许复用一行 240 像素 buffer。

## 现有基础

- LCD：`src/hal/st7789/st7789.h`
  - 可用 `St7789_Begin_Write()`、`St7789_Write_Pixels()`、`St7789_End_Write()` 连续写区域。
- 单行 buffer：`src/app/game_console/game_graphics.c`
  - `Game_Graphics_Get_Line_Buffer()` 返回 240 个 `uint16_t` 的共享行缓冲。
- 外部 Flash：
  - `src/app/storage/storage.c` 已挂载 W25Q32。
  - 高 2 MiB 是 LittleFS：`0x200000..0x3fffff`。
  - 低 2 MiB 可做 raw cache，但需要统一分配地址，避免和已有资源冲突。
- 图片资源样板：
  - `src/app/image_asset/image_asset.c` 定义了 `R565` 图片格式和逐行读取。
  - `src/app/air_battle/air_battle.c` 已展示“从 LittleFS/Raw cache 按行读背景，再写屏”的模式。
- 上传工具：
  - `scripts/flash_manager.py` 已支持 `upload`、`upload-image`。
  - 可扩展 `pack_video_asset()` 和 `upload-video`。

## 分支与配置

建议流程：

```powershell
git fetch
git switch -c feat-video
```

如果 `feat-video` 已存在：

```powershell
git switch feat-video
git pull
```

开发期建议使用独立固件或最小 app，减少 SRAM 压力：

```c
// config/app_config.h
#define LOW_KNIGHT_STANDALONE_ENABLE 0
#define FLASH_MGR_ENABLE 0
#define GAME_CONSOLE_ENABLE 1
#define GAME_RUNTIME_MONITOR_ENABLE 1
#define VIDEO_PLAYER_ENABLE 1
#define BOOT_ANIMATION_ENABLE 1
```

上传视频时切到 Flash Manager 固件：

```c
#define LOW_KNIGHT_STANDALONE_ENABLE 0
#define FLASH_MGR_ENABLE 1
#define GAME_CONSOLE_ENABLE 0
#define VIDEO_PLAYER_ENABLE 0
#define BOOT_ANIMATION_ENABLE 0
```

## 文件格式

新增容器格式建议命名为 `V565`。文件路径默认：

- 开机动画：`/boot.v565`
- 手动测试视频：`/demo.v565`

头部固定 32 字节，小端：

```c
typedef struct {
    char magic[4];          // "V565"
    uint8_t version;        // 1
    uint8_t flags;          // bit0: loop, bit1: reserved
    uint16_t width;         // 建议 240
    uint16_t height;        // 建议 320
    uint16_t fps_x100;      // 800 = 8 FPS
    uint16_t frame_count;
    uint16_t frame_format;  // 0 = raw RGB565 full frame
    uint32_t frame_bytes;   // width * height * 2
    uint32_t pixel_crc;     // 所有帧像素 CRC16 或 CRC32，PC 端生成
    uint32_t reserved0;
    uint32_t reserved1;
} Video_asset_header;
```

第一阶段只实现 `frame_format = 0`：

```text
32-byte header
frame 0 RGB565 little-endian, row-major
frame 1 RGB565 little-endian, row-major
...
```

容量估算：

- 240x320 RGB565 单帧：153,600 字节。
- 8 FPS、2 秒：约 2.46 MiB，不适合放在当前 2 MiB LittleFS。
- 160x128 RGB565 单帧：40,960 字节；8 FPS、3 秒：约 0.98 MiB。
- 实用建议：第一版先用 160x128 或 120x160，在 LCD 居中/等比放大或居中显示。

如果一定要全屏，后续再做 RLE/差分帧，不要第一版就上复杂压缩。

## PC 转换工具

在 `scripts/flash_manager.py` 增加：

```powershell
python scripts/flash_manager.py COM6 upload-video assets/videos/boot.mp4 /boot.v565 --width 160 --height 128 --fps 8 --fit contain
```

实现建议：

1. 依赖 `ffmpeg` 命令行，不在 Python 中手写视频解码。
2. 先用 `ffmpeg` 抽帧到临时目录：

```powershell
ffmpeg -i input.mp4 -vf "fps=8,scale=160:128:force_original_aspect_ratio=decrease,pad=160:128:(ow-iw)/2:(oh-ih)/2:black" frame_%05d.png
```

3. 用 Pillow 逐帧转 RGB565，小端写入 `V565`。
4. 复用现有 `FlashManager.upload_file()` 上传生成的临时 `.v565`。

先不要加音频。开机动画如果需要声音，之后由蜂鸣器单独播放短旋律。

## 运行时模块

新增目录：

```text
src/app/video_player/
  video_asset.h
  video_asset.c
  video_player.h
  video_player.c
```

`video_asset` 负责打开和按行读取：

```c
typedef struct {
#if FRAMEWORK_USE_LFS
    lfs_file_t file;
#endif
    uint16_t width;
    uint16_t height;
    uint16_t fps_x100;
    uint16_t frame_count;
    uint32_t frame_bytes;
    uint8_t flags;
    uint8_t is_open;
} Video_asset;

uint8_t Video_Asset_Open(Video_asset* video, const char* path);
void Video_Asset_Close(Video_asset* video);
uint8_t Video_Asset_Read_Line(Video_asset* video, uint16_t frame, uint16_t y, uint16_t* pixels);
```

`Video_Asset_Read_Line()` 只读取一行：

```c
offset = sizeof(Video_asset_header)
       + frame * frame_bytes
       + y * width * 2;
```

注意：

- 每次 `lfs_file_seek()` 和 `lfs_file_read()` 都要 `Storage_Lock()` / `Storage_Unlock()`。
- 可以缓存 `next_read_offset`，连续逐行播放时减少 seek。
- 如果视频不是 240 宽，先读入行首，然后居中显示；不要额外分配整行以外的大 buffer。

`video_player` 负责播放：

```c
typedef struct {
    St7789* lcd;
    const char* path;
    uint8_t loop;
    uint8_t allow_skip;
} Video_player_config;

uint8_t Video_Player_Play_Blocking(const Video_player_config* cfg);
```

第一版用 blocking 播放即可，开机动画最简单：

1. 打开 `/boot.v565`。
2. 校验宽高、fps、frame_count。
3. 对每帧：
   - 按帧率计算目标时间。
   - `St7789_Begin_Write()` 开一个视频显示区域。
   - 对每一行读入 `Game_Graphics_Get_Line_Buffer()`。
   - `St7789_Write_Pixels()` 写出。
   - `St7789_End_Write()`。
4. 文件不存在、格式错误、读取失败，立即返回 0。

显示区域建议：

- 如果 `width == 240 && height == 320`，全屏写。
- 否则居中显示，背景先清黑。
- 不做运行时缩放，缩放交给 PC 转换工具。

## 开机动画接入

当前入口：

- `src/main.c` 调 `App_Init()` 和 `App_Task_Def()`。
- `src/app/app.c` 创建具体 app 任务。

推荐接入点：在 `App_Task_Def()` 里，在游戏任务创建前创建一次性 boot video 任务。

建议新增：

```text
src/app/boot_animation/
  boot_animation.h
  boot_animation.c
```

职责：

1. 初始化 LCD。
2. 播放 `/boot.v565`。
3. 播完后删除自身任务，或直接继续创建主应用任务。

更稳妥的第一版：不要让 boot task 和 game task 同时拥有 LCD。可在 `App_Task_Def()` 中：

```c
#if BOOT_ANIMATION_ENABLE
    Boot_Animation_Play_Once();
#endif
#if GAME_CONSOLE_ENABLE
    Game_Console_Task_Def();
#endif
```

`Boot_Animation_Play_Once()` 是同步函数，内部临时创建 `St7789` 并播放。完成后再进入 Game Console。这样最少并发问题，但要确认再次 `St7789_Create/Init` 不会冲突。如果冲突，第二版再改成“Game Console 初始化 LCD 后先播放动画再 render menu”。

实际更推荐的长期结构：

- 把 LCD 创建下沉到一个共享 `display_service`，避免多个 app 重复创建 LCD。
- 本任务第一阶段可以先不重构。

## 作为可手动测试 app

为了方便调试，建议先注册一个菜单项：

```text
src/app/video_demo/
  video_demo.h
  video_demo.c
```

行为：

- 进入后播放 `/demo.v565`。
- 按确认键重播。
- 长按退出沿用 Game Console 的 `back_requested`。
- 文件不存在时显示 `NO /demo.v565`。

要改：

- `src/app/game_console/game_registry.h`
  - 增加 `game_icon_video` 和 `game_id_video`。
- `src/app/game_console/game_registry.c`
  - 注册 `"VIDEO"`。
- `src/app/game_console/game_console.c`
  - 给 `game_icon_video` 画一个简单图标，或先复用 `game_icon_air`。

## 性能建议

第一版目标：

- 160x128 @ 8 FPS，稳定播放。
- 每帧写 160 * 128 * 2 = 40 KiB，8 FPS 约 320 KiB/s，比较现实。

谨慎目标：

- 240x320 @ 5 FPS，约 768 KiB/s，可能受软件 SPI/LittleFS seek 影响明显。

不要做：

- 每行都重新打开文件。
- 每帧 `malloc`。
- 在播放时同时跑游戏主循环或频繁播放蜂鸣器复杂音乐。

优化路径：

1. 连续读：`next_read_offset` 命中时跳过 seek。
2. Raw cache：把 `/boot.v565` 的像素区复制到 W25Q32 低 2 MiB 的 raw 区，然后用 `Storage_Raw_Read()` 顺序读。
3. 压缩：增加简单 RLE 行格式，适合黑底 logo 动画。
4. 差分：I 帧 + dirty rectangle/P 帧，适合开机 logo 移动。

## Raw cache 地址规划

已有：

```c
#define AIR_BATTLE_BG_CACHE_ADDRESS  (1u * 1024u * 1024u)
#define AIR_BATTLE_BG_CACHE_CAPACITY (256u * 1024u)
```

建议新增：

```c
#define VIDEO_BOOT_CACHE_ADDRESS     (128u * 1024u)
#define VIDEO_BOOT_CACHE_CAPACITY    (768u * 1024u)
```

注意低 2 MiB 是共享 raw 区，提交前必须在 `config/app_config.h` 注释里写清楚地址分配表。

## 验收标准

必须完成：

1. `python scripts/flash_manager.py COMx upload-video ... /boot.v565` 可生成并上传视频。
2. 固件启动后，如果 `/boot.v565` 存在且格式正确，会播放一次。
3. `/boot.v565` 不存在、损坏、尺寸不支持时，固件正常进入原应用。
4. 播放期间不崩溃、不明显撕裂，不使用大块 SRAM。
5. `cmake --build build_mingw -j 8` 通过。

建议验证：

1. 上传不同视频，不重新烧录固件，重启后内容改变。
2. 分别测试 160x128 @ 8 FPS、240x320 @ 5 FPS。
3. 打开 `GAME_RUNTIME_MONITOR_ENABLE`，观察 heap 和 stack。
4. 使用 `python scripts/flash_manager.py COMx info /boot.v565` 确认文件大小。

## 推荐实施顺序

1. 新建 `docs/video_playback_plan_zh.md` 之外的实现文件，先提交空模块骨架。
2. 扩展 `scripts/flash_manager.py`：实现本地 `pack-video` 或 `upload-video`。
3. 实现 `Video_Asset_Open/Read_Line/Close`。
4. 实现 `Video_Player_Play_Blocking()`，只支持 240x320 或居中原尺寸。
5. 做 `video_demo` 菜单项，先手动播放 `/demo.v565`。
6. 接入 `Boot_Animation_Play_Once()`。
7. 再考虑 raw cache 和压缩格式。

## 常见坑

- 240x320 RGB565 一帧 153,600 字节，SRAM 绝对放不下整帧。
- 当前 LCD 是软件 SPI 场景，实际 FPS 可能比理论低，先用小尺寸。
- LittleFS 高 2 MiB 容量有限，全屏原始视频很快超过容量。
- `Game_Graphics_Get_Line_Buffer()` 是共享静态 buffer，不要跨任务同时写 LCD。
- 开机动画不要阻塞 Flash Manager 上传固件；上传固件应关闭 boot animation。
- Windows 下 `ffmpeg` 不一定在 PATH，脚本需要给出清晰错误提示。

