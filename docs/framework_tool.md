# Framework 开发者工具

`scripts/framework.py` 提供面向开发者的诊断入口，用于检查构建 target、
主机工具以及链接 map 文件中的内存区域摘要。

## 命令

```bash
python3 scripts/framework.py doctor
python3 scripts/framework.py inspect
python3 scripts/framework.py size build/arm/framework.map
```

## 环境诊断

`doctor` 检查主机工具、Python 包、ARM/VM 构建依赖是否就绪。
Note 列仅在检查未通过时显示该项的用途，通过时留空。

当前检查项：

- `PATH` 中是否可以找到 `python3`、`cmake` 和 `ninja`
- Python 包是否可导入：`PyYAML`、`pyserial`、`Pillow`、`numpy`
- VM target 是否可以找到 `sdl2-config`
- ARM target 是否可以在配置路径下找到 `arm-none-eabi-gcc`、`arm-none-eabi-g++`、`arm-none-eabi-objcopy`、`arm-none-eabi-size`
- ARM target 是否可以在配置路径下找到 `sysconfig_cli`、`sysconfig_cli.sh` 或 `sysconfig_cli.bat`

状态符号：

| 符号 | 颜色 | 含义 |
| --- | --- | --- |
| `✓` | 绿 | 当前检查通过 |
| `⚠` | 黄 | 可选依赖缺失，不影响不使用该功能的构建 |
| `✗` | 红 | 当前配置下必需依赖缺失或配置错误，`doctor` 返回非零退出码 |

示例：

```text
  Check                            Note
  ───────────────────────────────  ────────────────────────────
  ✓  python3
  ✓  cmake
  ⚠  python package pyserial        Flash Manager 和串口调试脚本
  ✗  arm toolchain arm-none-eabi-gcc  构建 ARM target 'arm'
```

## ARM 工具路径配置

ARM target 支持三个配置项：

```yaml
build:
  - name: arm
    platform: ARM
    arm_tool_chain_path: ""
    sysconfig_path: ""
    skip_syscfg: OFF
```

`arm_tool_chain_path` 为空字符串时使用 `tools/gcc-arm-none-eabi`，否则使用填写的路径。
`sysconfig_path` 为空字符串时使用 `tools/sysconfig`，否则使用填写的路径。
`skip_syscfg: ON` 跳过首次编译时自动调用 SysConfig 生成硬件配置文件的步骤。
相对路径按仓库根目录解析，绝对路径保持不变。

`scripts/cc.py` 会把解析后的路径传给 CMake：

```text
-DARM_TOOLCHAIN_ROOT=<resolved-path>
-DSYSCONFIG_ROOT=<resolved-path>
-DSKIP_SYSCFG=ON|OFF
```

`doctor` 使用同一套解析规则检查路径下的工具是否存在。

## Target 检视

`inspect` 打印从 `config/config.yaml` 转发给 CMake 的实际 `-D` 参数。
当某个 target 的构建行为与预期不一致时，先用它确认开关是否正确传入。

```text
arm (ARM, MinSizeRel, ninja)
  -DFRAMEWORK_USE_FREERTOS=ON
  -DFRAMEWORK_USE_LFS=ON
  -DFRAMEWORK_USE_UART=ON
```

## Map 摘要

`size` 读取链接器 `.map` 文件，打印内存区域的起始地址和长度。
第一版保持很小，只提供基础摘要；后续可以继续扩展 section 排名、
对象文件占用排行和 Flash/RAM 风险提示。
