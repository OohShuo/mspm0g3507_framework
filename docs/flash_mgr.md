# SPI Flash 管理 · Flash Manager

通过 PC 管理 W25Q32 外部 SPI Flash，包括 LittleFS 文件系统操作、图片/视频资源上传、raw 区域直接读写。

## 硬件概述

| 芯片 | 容量 | 接口 | 用途 |
|------|------|------|------|
| W25Q32 | 4 MiB (32 Mbit) | SPI | LittleFS 文件系统 + raw 资源缓存区 |

### Flash 地址布局

```
W25Q32 (4 MiB)
┌──────────────────────┐ 0x000000
│  Raw 区域 (1 MiB)     │  图片缓存、资源直接寻址
├──────────────────────┤ 0x100000
│                      │
│  LittleFS (3 MiB)    │  文件系统：游戏资源、分数、BARD 视频
│                      │
└──────────────────────┘ 0x400000
```

- **Raw 区域**：通过 `Storage_Raw_*` API 直接按地址读写，用于 `Image_Asset` 缓存等不需要文件系统的场景
- **LittleFS**：标准文件系统，通过 Flash Manager 协议在 PC 端远程管理

## 编译配置

### 固件端开关

编辑 `config/config.yaml` 的 ARM target：

```yaml
- name: arm
  platform: ARM
  FRAMEWORK_USE_LFS: ON    # 启用 LittleFS 文件系统
  FRAMEWORK_USE_UART: ON   # 启用 UART0（Flash Manager 通信口）
```

编辑 `config/app_config.h`：

```c
#define FLASH_MGR_ENABLE  1   // 启用 Flash Manager 后台任务
```

三个开关必须同时打开，缺少任意一个 Flash Manager 都不会启动：

| 开关 | 文件 | 作用 |
|------|------|------|
| `FRAMEWORK_USE_LFS: ON` | `config.yaml` | 编译 LittleFS 和 Storage 模块 |
| `FRAMEWORK_USE_UART: ON` | `config.yaml` | 编译 UART0 DMA 驱动和 Com_uart HAL |
| `FLASH_MGR_ENABLE 1` | `app_config.h` | 创建 Flash Manager FreeRTOS 任务 |

`FRAMEWORK_USE_LFS` 和 `FRAMEWORK_USE_UART` 通过 `scripts/cc.py` 转为 CMake `-D` 参数，最终以 `#define` 形式传给编译器。

## 编译与烧录

```bash
# 1. 构建 ARM 固件
python3 scripts/cc.py --target arm

# 2. 通过 pyOCD + DAP-Link 烧录
bash scripts/flash.bash
```

`flash.bash` 等价于：

```bash
pyocd flash --target MSPM0G3507 --base-address 0x00000000 build/arm/framework.elf
pyocd reset --target MSPM0G3507
```

> 如果之前 Flash 布局发生过变化（如 LFS 分区大小调整），烧录后需要通过 Flash Manager 重新格式化 LittleFS。

## PC 端工具

依赖安装：

```bash
pip install pyserial pillow numpy
sudo apt install ffmpeg    # 仅视频编码需要
```

### 交互式工作台（推荐）

无参数运行进入菜单：

```bash
python3 scripts/flash_manager.py
```

菜单功能：

| 按键 | 功能 |
|------|------|
| `O` | 选择串口并连接设备 |
| `L` | 列出远端目录 |
| `U` | 上传文件（带进度条和速度） |
| `D` | 下载文件 |
| `R` | 删除文件（需输入 `yes` 确认） |
| `F` | 格式化 LittleFS（需输入 `FORMAT` 确认） |
| `I` | 查看设备 / 文件信息 |
| `P` | 枚举可用串口 |
| `C` | 查看 / 修改运行时配置 |
| `H` | 帮助说明 |
| `Q` | 退出 |

连接后在 banner 中会显示 `port`、`baudrate`、`local cwd`、`remote cwd` 和连接状态。

### CLI 模式

```bash
# 列出串口
python3 scripts/flash_manager.py ports

# 探测设备
python3 scripts/flash_manager.py /dev/ttyUSB0 probe

# 文件操作
python3 scripts/flash_manager.py /dev/ttyUSB0 list /
python3 scripts/flash_manager.py /dev/ttyUSB0 upload build/res.bin /res.bin
python3 scripts/flash_manager.py /dev/ttyUSB0 download /res.bin ./res.bin
python3 scripts/flash_manager.py /dev/ttyUSB0 delete /res.bin
python3 scripts/flash_manager.py /dev/ttyUSB0 info /res.bin

# 格式化
python3 scripts/flash_manager.py /dev/ttyUSB0 format --yes
```

### 图片转换上传

将 JPG/PNG 转为 RGB565 格式并上传到 LittleFS：

```bash
# 交互式：启动工作台后用 [U] 选择 .r565 文件上传
python3 scripts/flash_manager.py /dev/ttyUSB0 upload-image \
  assets/images/bg.jpg /bg.r565 --width 240 --height 320 --fit cover
```

也可以分两步（先转换再上传）：

```bash
python3 scripts/img2r565.py assets/images/bg.jpg -W 240 -H 320 --fit cover -o /tmp/bg.r565
python3 scripts/flash_manager.py /dev/ttyUSB0 upload /tmp/bg.r565 /bg.r565
```

`img2r565.py` 参数：

| 参数 | 说明 |
|------|------|
| `input` | JPG / PNG 文件 |
| `-o` | 输出 `.r565` 路径 |
| `-W` / `-H` | 目标宽高（ST7789 竖屏为 240×320） |
| `--fit` | `cover`（裁剪）、`contain`（留边）、`stretch`（拉伸） |
| `--mask` | 同时生成 1-bit 透明度遮罩 |

### 视频编码上传

将视频转为 BARD 矩形差分格式：

```bash
python3 scripts/encode_video_rect_delta.py \
  --input assets/bad_apple.mp4 \
  --out-bard assets/bad_apple.bard \
  --width 240 --height 180 \
  --display-x 0 --display-y 70 \
  --fps 15 \
  --threshold 128

python3 scripts/flash_manager.py /dev/ttyUSB0 upload assets/bad_apple.bard /bad_apple.bard
```

BARD 编码参数：

| 参数 | 说明 | 建议值 |
|------|------|--------|
| `--input` | 输入视频 | |
| `--out-bard` | 输出 `.bard` 路径 | |
| `--fps` | 帧率 | 15 即可，24 效果更好但文件更大 |
| `--width` / `--height` | 编码分辨率（max 256） | 240×180 适配屏幕 |
| `--display-x` / `--display-y` | 显示偏移 | 居中时 `0` / `70` |
| `--threshold` | 二值化阈值 0-255 | 128 |
| `--max-seconds` | 只编码前 N 秒（测试用） | |
| `--invert` | 黑白反转 | 按需 |

## 通信协议

Flash Manager 使用基于 UART 的二进制帧协议：

```
帧格式：SYNC0(0xAA) SYNC1(0x55) CMD SEQ_L SEQ_H LEN_L LEN_H DATA[0..511] CRC_L CRC_H
```

- 每个 chunk 最多 512 字节有效载荷
- 每帧带 16-bit CCITT CRC 校验
- 超时 / CRC 错误自动重传（最多 3 次）
- 文件通过多次 WRITE 命令按 offset 分片上传

协议命令：

| 命令 | 代码 | 说明 |
|------|------|------|
| READ | 0x01 | 读文件 |
| WRITE | 0x02 | 写文件（open → seek → write → close per chunk） |
| DELETE | 0x03 | 删除文件 |
| LIST | 0x04 | 列目录 |
| INFO | 0x05 | 文件信息 |
| FORMAT | 0x06 | 格式化 LittleFS |
| RESET | 0x07 | 复位协议状态机 |

设备响应码：

| 响应 | 代码 | 说明 |
|------|------|------|
| ACK | 0x80 | 成功 |
| NAK | 0x81 | 错误（附带错误码） |
| CRC_ERR | 0x82 | 收到 CRC 错误帧 |
| BUSY | 0x83 | 设备忙（互斥锁占用） |
| CHUNK | 0x84 | 读数据块 |
| EOF | 0x85 | 读结束 |
| LIST_ITEM | 0x86 | 目录条目 |
| LIST_END | 0x87 | 目录结束 |
| INFO_RESP | 0x88 | 文件信息响应 |

## 串口连接

设备通过 UART0 与 PC 通信，使用 MSPM0G3507 板载 DAP-Link 虚拟串口。Linux 下通常为 `/dev/ttyUSB0`，Windows 下为 `COM3` 等。

PC 端连接时自动使用 `config["baudrate"]` 指定的波特率（默认 2000000，可在交互式工作台 `[C]` 菜单修改）。
