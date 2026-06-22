# 脚本说明 · Scripts

`scripts/` 目录用于放置项目开发过程中的命令行工具。清理后的原则是：**核心流程保留，低频工具归类，实验脚本隔离，生成物和大安装包不提交。**

## 目录结构

```text
scripts/
├── README.md
├── cc.py
├── cm.bash
├── cm.cmd
├── clear.bash
├── flash.bash
├── gen_syscfg_files.bash
├── open_syscfg_gui.bash
├── install_sysconfig.bash
├── flash_manager.py
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
| `flash.bash` | 使用 pyOCD 烧录 `build/arm/framework.elf` 并复位运行 | 高 |
| `serve_docs.sh` | 本地预览或构建 MkDocs 文档站点 | 中 |

## SysConfig 脚本

| 脚本 | 作用 | 说明 |
| --- | --- | --- |
| `open_syscfg_gui.bash` | 打开 TI SysConfig GUI | 修改引脚、时钟、外设配置时使用 |
| `gen_syscfg_files.bash` | 从 `.syscfg` 重新生成 `config/syscfg/` | 只复制 `ti_msp_dl_config.c/.h` 和 `device.opt` |

## 串口与 Flash 工具

| 脚本 | 作用 | 说明 |
| --- | --- | --- |
| `flash_manager.py` | PC 端外部 Flash / LittleFS 管理工具 | 支持上传、下载、删除、格式化、图片转换上传 |
| `com_uart_test.py` | UART 收发测试脚本 | - |

示例：

```bash
python3 scripts/com_uart_test.py --port /dev/ttyUSB0 --baud 115200
python3 scripts/flash_manager.py /dev/ttyUSB0 list
python3 scripts/flash_manager.py /dev/ttyUSB0 format --yes
```

## 资源生成与实验脚本

| 路径 | 作用 | 说明 |
| --- | --- | --- |
| `generate_air_battle_assets.py` | 生成飞机大战贴图资源 | 修改飞机大战贴图后运行 |
| `generate_info_images.py` | 生成 Info 页面图片资源 | 修改 Info 图片后运行 |
| `assets/LVGLImage.py` | LVGL 图片转换工具 | 当前 LVGL 默认未启用，低频使用 |
| `experimental/slip_send.py` | SLIP/7D7E 协议实验发送脚本 | 不属于主流程，保留在实验目录 |
