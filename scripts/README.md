# scripts 目录说明

这个目录只保留项目日常开发需要的脚本，生成物、安装包和重复测试脚本已清理。

## 常用脚本

| 脚本 | 作用 | 建议 |
| --- | --- | --- |
| `cc.py` | 读取 `config/config.yaml`，按 target 调用 CMake 构建 | 核心脚本，保留 |
| `cm.bash` | Linux/macOS 构建入口，转发到 `cc.py` | 保留 |
| `cm.cmd` | Windows 构建入口，转发到 `cc.py` | 已修正，保留 |
| `clear.bash` | 删除 `build/` | 保留 |
| `flash.bash` | 使用 pyOCD 烧录 `build/arm/framework.elf` | 保留 |
| `gen_syscfg_files.bash` | 调用 TI SysConfig CLI 生成 `config/syscfg/` 文件 | 保留 |
| `open_syscfg_gui.bash` | 打开 TI SysConfig GUI | 已修正 shebang，保留 |
| `install_sysconfig.bash` | 静默安装 TI SysConfig | 安装包不再放在 `scripts/` 下 |
| `flash_manager.py` | PC 端外部 Flash / LittleFS 串口管理工具 | 保留 |
| `com_uart_test.py` | 与固件端 com_uart 测试配套的串口收发脚本 | 保留 |
| `generate_air_battle_assets.py` | 生成飞机大战资源头文件 | 保留 |
| `generate_info_images.py` | 生成 info 页面图片资源 | 保留 |
| `serve_docs.sh` | 本地预览或构建 MkDocs 文档站点 | 保留 |

## 非日常脚本

| 路径 | 作用 | 说明 |
| --- | --- | --- |
| `assets/LVGLImage.py` | LVGL 图片转换工具 | 当前 LVGL 默认未启用，移入资源工具目录 |
| `experimental/slip_send.py` | SLIP/7D7E 串口协议实验发送脚本 | 非主流程，移入实验目录 |

## 常用命令

```bash
# 构建 VM
python3 scripts/cc.py --target vm

# 构建 ARM
python3 scripts/cc.py --target arm

# 清理构建目录
bash scripts/clear.bash

# 生成 SysConfig 文件
bash scripts/gen_syscfg_files.bash

# 打开 SysConfig GUI
bash scripts/open_syscfg_gui.bash

# 烧录 ARM 固件
bash scripts/flash.bash

# 预览文档
bash scripts/serve_docs.sh
```
