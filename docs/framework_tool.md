# Framework 开发者工具

`scripts/framework.py` 提供面向开发者的诊断入口，用于检查构建 target、
主机工具、Flash Manager 开关一致性，以及链接 map 文件中的内存区域摘要。

## 命令

```bash
python3 scripts/framework.py doctor
python3 scripts/framework.py inspect
python3 scripts/framework.py size build/arm/framework.map
```

## 环境诊断

`doctor` 检查主机工具、Python 包、ARM/VM 构建依赖和高层配置是否一致。
每条结果都会说明“必要性”和“何时需要”，方便判断当前缺失项是否会影响手头工作。

当前检查项：

- `PATH` 中是否可以找到 `python3`、`cmake` 和 `ninja`
- Python 包是否可导入：`PyYAML`、`pyserial`、`Pillow`、`numpy`
- VM target 是否可以找到 `sdl2-config`
- ARM target 是否可以在配置路径下找到 `arm-none-eabi-gcc`、`arm-none-eabi-g++`、`arm-none-eabi-objcopy`、`arm-none-eabi-size`
- ARM target 是否可以在配置路径下找到 `sysconfig_cli`、`sysconfig_cli.sh` 或 `sysconfig_cli.bat`
- `FLASH_MGR_ENABLE` 是否与 ARM target 开关一致
- 启用 Flash Manager 时，`FRAMEWORK_USE_LFS` 和 `FRAMEWORK_USE_UART` 是否同时打开

状态含义：

| 状态 | 含义 |
| --- | --- |
| `[OK]` | 当前检查通过 |
| `[WARN]` | 可选依赖缺失，不影响不使用该功能的构建 |
| `[ERR]` | 当前配置下必需依赖缺失或配置错误，`doctor` 返回非零退出码 |

示例：

```text
[OK] python3: /usr/bin/python3；必要性：必需；何时需要：运行项目脚本
[OK] cmake: /usr/bin/cmake；必要性：必需；何时需要：配置和生成所有构建 target
[WARN] python package pyserial: pyserial 未安装；必要性：可选；何时需要：Flash Manager 和串口调试脚本
[ERR] arm toolchain arm-none-eabi-gcc: /repo/tools/gcc-arm-none-eabi/bin/arm-none-eabi-gcc 不存在；必要性：必需；何时需要：构建 ARM target 'arm'
```

## ARM 工具路径配置

ARM target 支持两个路径配置项：

```yaml
build:
  - name: arm
    platform: ARM
    arm_tool_chain_path: ""
    sysconfig_path: ""
```

`arm_tool_chain_path` 为空字符串时使用 `tools/gcc-arm-none-eabi`，否则使用填写的路径。
`sysconfig_path` 为空字符串时使用 `tools/sysconfig`，否则使用填写的路径。
相对路径按仓库根目录解析，绝对路径保持不变。

`scripts/cc.py` 会把解析后的路径传给 CMake：

```text
-DARM_TOOLCHAIN_ROOT=<resolved-path>
-DSYSCONFIG_ROOT=<resolved-path>
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
