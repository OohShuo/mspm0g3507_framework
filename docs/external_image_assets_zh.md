# 外部 Flash 图片资源使用指南

项目可以把 JPG/PNG 图片在电脑端转换为 MCU 可直接读取的 `R565`
格式，再通过 UART 上传到 W25Q32 的 LittleFS 分区。运行时按行读取图片，
不需要在 MCU 上解码 JPG/PNG，也不需要把整张图片放进 SRAM。

[English version](external_image_assets.md)

## 存储容量

- W25Q32 总容量为 4 MiB。
- 当前 LittleFS 使用高地址的 2 MiB 分区：`0x200000` 到 `0x3fffff`。
- 飞机大战使用低地址保留区中的 `0x100000` 到 `0x13ffff` 作为高清背景原始缓存槽。
- 一张不透明的 240x320 RGB565 图片占用 153,616 字节。
- 当前分区大约可以保存 13 张全屏不透明图片。
- 扩大分区前必须确认低地址 2 MiB 没有保留数据，并重新格式化文件系统。

## 第一次准备

安装电脑端依赖：

```powershell
pip install pyserial pillow
```

查看电脑当前识别到的串口：

```powershell
python scripts/flash_manager.py --list-ports
```

如果有多个串口，可以拔掉设备后再次执行命令，通过前后差异确认端口号。
端口号可能在重新插拔后发生变化，不一定始终是 `COM3`。

当前使用 CMSIS-DAP/DAPLink 调试器时，其虚拟串口通常显示为
`VID:PID=0D28:0204`。在当前电脑上对应 `COM6`。SWD 接线只负责烧录和调试，
UART 仍需单独连接：

| MCU | 调试器 UART |
|---|---|
| `PA10 / UART0 TX` | `RX` |
| `PA11 / UART0 RX` | `TX` |
| `GND` | `GND` |

TX 与 RX 交叉连接，使用 3.3V TTL 电平。通常不需要连接调试器的 VCC。

> SysConfig 中 `DMA_CH3`、`DMA_CH4` 是配置对象名称，不等同于最终硬件通道号。
> 当前生成结果为 UART0 TX 使用硬件 DMA3，UART0 RX 使用硬件 DMA4。

## 上传图片

UART 文件管理器和游戏主程序采用两套固件配置，以便给游戏保留更多 SRAM。

1. 在 `config/app_config.h` 中临时启用上传固件：

   ```c
   #define FLASH_MGR_ENABLE 1
   #define GAME_CONSOLE_ENABLE 0
   ```

2. 编译并烧录上传固件：

   ```powershell
   python scripts/cm.py
   ```

3. 烧录完成后，关闭可能持续占用串口的串口终端或监视器。重新确认端口：

   ```powershell
   python scripts/flash_manager.py --list-ports
   python scripts/flash_manager.py COM6 probe
   ```

4. 转换并上传图片：

   ```powershell
   python scripts/flash_manager.py COM6 upload-image assets/images/bg2.jpg /air_bg.r565 --width 240 --height 320 --fit cover
   ```

5. 查看文件是否已经写入：

   ```powershell
   python scripts/flash_manager.py COM6 list
   python scripts/flash_manager.py COM6 info /air_bg.r565
   ```

6. 上传完成后，在 `config/app_config.h` 中恢复游戏固件：

   ```c
   #define FLASH_MGR_ENABLE 0
   #define GAME_CONSOLE_ENABLE 1
   ```

7. 再次编译并烧录。烧录 MCU 内部 Flash 不会擦除外部 W25Q32 中的图片。

## 图片转换参数

- `--fit cover`：保持比例并裁剪，铺满目标尺寸，适合全屏背景。
- `--fit contain`：保持比例并完整显示，空白区域透明，适合精灵贴图。
- `--fit stretch`：直接拉伸到目标尺寸，可能产生变形。
- `--mask`：保存 1 bit 透明蒙版，通常与 `--fit contain` 一起使用。
- `--width`、`--height`：输出图片尺寸，必须与游戏读取时要求的尺寸一致。

`R565` 文件包含 16 字节头、按行排列的小端 RGB565 像素，以及可选的
逐行 1 bit 透明蒙版。运行时接口位于
`src/app/image_asset/image_asset.h`。

## 飞机大战

飞机大战会优先读取 `/air_bg.r565` 作为 240x320 背景。如果文件不存在、
损坏或尺寸不正确，会自动使用固件内置的低分辨率背景，不会阻止游戏启动。

为了避免游戏过程中反复经过 LittleFS 定位小块背景，首次进入飞机大战时，
固件会把图片像素复制到 W25Q32 低地址保留区的原始缓存槽。首次建立缓存可能
需要等待几秒；以后进入游戏会直接复用，不再自动降低画质。游戏只把当前脏区
的一行像素读入 480 字节共享缓冲，不会把整张图片载入 SRAM。

新版 `flash_manager.py` 会把图片内容指纹写入 R565 文件头。重新上传不同图片后，
游戏会发现指纹变化并自动重建原始缓存。旧版已经上传、指纹为 0 的图片也可以
正常建立和使用缓存。

缓存位置可在 `config/app_config.h` 中配置：

```c
#define AIR_BATTLE_BG_CACHE_ADDRESS  (1u * 1024u * 1024u)
#define AIR_BATTLE_BG_CACHE_CAPACITY (256u * 1024u)
```

不要把这个地址范围同时分配给其他原始 Flash 资源。格式化 LittleFS 只影响高
2 MiB 文件系统分区，不会清除这个原始缓存；缓存失效时会由游戏自动覆盖。

## 常见问题

### `PermissionError(13, '拒绝访问。')`

该错误发生在图片转换和 UART 协议通信之前，表示 Windows 不允许 Python
打开串口。串口通常正被另一个程序独占。

依次检查：

1. 关闭串口助手、VS Code 串口监视器、CCS/UniFlash 的串口终端。
2. 确认没有另一个 `flash_manager.py` 或 Python 串口程序仍在运行。
3. 重新插拔 USB，运行 `--list-ports` 确认最新端口号。
4. 必要时关闭并重新打开使用过串口的 IDE；不需要首先修改图片参数。

### 能打开串口，但显示 `FAILED` 或设备无响应

检查当前烧录的是否为上传固件：

```c
#define FLASH_MGR_ENABLE 1
#define GAME_CONSOLE_ENABLE 0
```

同时确认波特率为 921600、选择的是连接 MCU UART 的端口，并在烧录后复位
一次开发板。COM 口能被电脑识别，不代表当前 MCU 固件正在运行文件管理协议。

### 提示 `filesystem corruption`

先烧录包含 SPI DMA 完成等待修复的最新上传固件，然后显式格式化当前使用的
高 2 MiB LittleFS 分区：

```powershell
python scripts/flash_manager.py COM6 format --yes
python scripts/flash_manager.py COM6 list
```

格式化会删除该 LittleFS 分区中的所有图片、成绩和其他文件，但不会擦除
W25Q32 低 2 MiB 保留区。完成后重新执行图片上传命令。

### 提示缺少模块

```powershell
pip install pyserial pillow
```

`pyserial` 用于串口通信，`Pillow` 用于 JPG/PNG 转换。
