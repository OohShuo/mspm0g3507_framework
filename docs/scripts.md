# 脚本说明 · Scripts

`scripts/` 目录用于放置项目开发过程中的命令行工具。

## 目录结构

```text
scripts/
├── README.md
├── cc.py
├── cm.bash
├── cm.cmd
├── clear.bash
├── format.bash
├── flash.bash
├── gen_syscfg_files.bash
├── open_syscfg_gui.bash
├── install_sysconfig.bash
├── flash_manager.py
├── img2r565.py
├── r5652img.py
├── mk_vm_flash.py
├── encode_video_rect_delta.py
├── flashmgr/
│   ├── __init__.py
│   ├── client.py
│   ├── utils.py
│   ├── cli.py
│   └── ui.py
├── com_uart_test.py
├── generate_air_battle_assets.py
├── generate_info_images.py
├── serve_docs.sh
├── assets/
│   └── LVGLImage.py
└── experimental/
    └── slip_send.py
```

## 日常脚本

| 脚本 | 作用 | 常用程度 |
| --- | --- | --- |
| `cc.py` | 读取 `config/config.yaml`，调用 CMake 构建 ARM / VM target | 高 |
| `cm.bash` | Linux/macOS 构建包装脚本，转发到 `cc.py` | 高 |
| `cm.cmd` | Windows 构建包装脚本，转发到 `cc.py` | 中 |
| `clear.bash` | 删除 `build/` 构建目录 | 中 |
| `format.bash` | 使用 clang-format 格式化 `src/`、`lib/local_lib/`、`config/` 的 C 源文件 | 中 |
| `flash.bash` | 使用 pyOCD 烧录 `build/arm/framework.elf` 并复位运行 | 高 |
| `serve_docs.sh` | 本地预览或构建 MkDocs 文档站点 | 中 |

## SysConfig 脚本

| 脚本 | 作用 | 说明 |
| --- | --- | --- |
| `open_syscfg_gui.bash` | 打开 TI SysConfig GUI | 修改引脚、时钟、外设配置时使用 |
| `gen_syscfg_files.bash` | 从 `.syscfg` 重新生成 `config/syscfg/` | 只复制 `ti_msp_dl_config.c/.h` 和 `device.opt` |

## 串口与 Flash 工具

> 完整的使用指南、编译配置和协议说明见 **[SPI Flash 管理](flash_mgr.md)**。

| 脚本 | 作用 | 说明 |
| --- | --- | --- |
| `flash_manager.py` | PC 端外部 Flash / LittleFS 管理工具 | 交互式工作台 + CLI 两种模式；支持上传、下载、删除、格式化、图片转换上传 |
| `img2r565.py` | 图片转 RGB565 格式 | 离线转换 JPG/PNG 为 `.r565`，无需连接设备 |
| `encode_video_rect_delta.py` | 视频转 BARD 矩形差分格式 | 将 MP4 编码为固件可渲染的 `.bard` 文件 |
| `flashmgr/` | flash_manager 的 Python 包 | `client.py`(协议)、`ui.py`(交互菜单)、`cli.py`(命令行)、`utils.py`(输出工具) |
| `com_uart_test.py` | UART 收发测试脚本 | - |

### Flash Manager Workbench

无参数运行时进入交互式工作台：

```bash
python3 scripts/flash_manager.py
```

进入后可以：

- `[O]` 选择串口并连接设备
- `[I]` 查看设备 / 文件系统信息
- `[L]` 列出远端目录
- `[U]` 上传文件（带进度和速度显示）
- `[D]` 下载文件（带进度和速度显示）
- `[R]` 删除文件（需输入 `yes` 确认）
- `[F]` 格式化文件系统（需输入 `FORMAT` 确认）
- `[P]` 枚举可用串口
- `[C]` 查看 / 修改运行时配置
- `[H]` 帮助说明
- `[Q]` 退出

带参数运行时保持原有 CLI 行为：

```bash
# 列出可用串口
python3 scripts/flash_manager.py --list-ports
python3 scripts/flash_manager.py ports

# 串口 + 操作
python3 scripts/flash_manager.py /dev/ttyUSB0 probe
python3 scripts/flash_manager.py /dev/ttyUSB0 list
python3 scripts/flash_manager.py /dev/ttyUSB0 upload build/res.bin /res.bin
python3 scripts/flash_manager.py /dev/ttyUSB0 download /res.bin ./res.bin
python3 scripts/flash_manager.py /dev/ttyUSB0 delete /res.bin
python3 scripts/flash_manager.py /dev/ttyUSB0 info /
python3 scripts/flash_manager.py /dev/ttyUSB0 format --yes

# 图片转换上传
python3 scripts/flash_manager.py /dev/ttyUSB0 upload-image photo.jpg /bg.r565 --width 240 --height 135

# UART 测试
python3 scripts/com_uart_test.py --port /dev/ttyUSB0 --baud 2000000
```

## 资源转换工具

| 脚本 | 作用 | 说明 |
| --- | --- | --- |
| `img2r565.py` | JPG/PNG 转 RGB565 格式 | 离线转换，输出 `.r565` 文件，可通过 `flash_manager.py` 上传 |
| `r5652img.py` | RGB565 转回 JPG/PNG | 将 `.r565` 文件解码为图片，方便预览和调试 |
| `encode_video_rect_delta.py` | 视频转 BARD 矩形差分格式 | 将 MP4 编码为固件可渲染的 `.bard` 文件 |
| `generate_air_battle_assets.py` | 生成飞机大战贴图资源 | 修改飞机大战贴图后运行 |
| `generate_info_images.py` | 生成 Info 页面图片资源 | 修改 Info 图片后运行 |
| `mk_vm_flash.py` | 构建 VM LittleFS 闪存镜像 | 将 `assets/vm_flash/` 打包为 `.bin` 镜像（当前 VM 已改用宿主机文件系统直接读写，此脚本为可选工具） |
| `assets/LVGLImage.py` | LVGL 图片转换工具 | 当前 LVGL 默认未启用，低频使用 |

## 实验脚本

| 路径 | 作用 | 说明 |
| --- | --- | --- |
| `experimental/slip_send.py` | SLIP/7D7E 协议实验发送脚本 | 不属于主流程，保留在实验目录 |
