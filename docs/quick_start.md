# 快速开始

> 建议在 Linux 下使用，跨平台不保证能够顺利运行，Windows 环境配置详见 [Windows 环境配置](win_env.md)。

本文档用于从零构建和运行 MSPM0G3507 Framework。建议先构建 VM，再构建 ARM 固件。

## 环境准备

Ubuntu 示例：

```bash
sudo apt update
sudo apt install -y python3 python3-yaml cmake ninja-build libsdl2-dev
```

文档站点依赖：

```bash
python3 -m pip install -r requirements-docs.txt
```

下载 ARM Embedded Toolchain，解压到 `tools/gcc-arm-none-eabi` 目录；VM 目标使用系统 GCC/Clang 与 SDL2。

下载 TI SysConfig 工具，解压到 `tools/sysconfig` 目录。

## 构建配置

构建目标由 `config/config.yaml` 管理：

```yaml
build:
  - name: vm
    platform: VM
    build_type: Release
    generator: ninja

  - name: arm
    platform: ARM
    build_type: MinSizeRel
    generator: ninja
```

其中：

- `name` 是构建目标名，对应 `--target` 参数。
- `platform` 决定使用 ARM 平台还是 VM 平台。
- `build_type` 传递给 CMake，例如 `Debug`、`Release`、`RelWithDebInfo`、`MinSizeRel`。
- `FRAMEWORK_USE_*` 字段会被 `scripts/cc.py` 转换成 CMake 定义。

不建议直接手写 `cmake` 命令，因为这样容易绕过 `config/config.yaml` 中的功能开关。

## 构建 VM

```bash
python3 scripts/cc.py --target vm
./build/vm/framework_vm
```

VM 用 SDL2 模拟显示、输入、音频和振动反馈，适合调试游戏逻辑与 UI。

## 构建 ARM

```bash
python3 scripts/cc.py --target arm
```

构建完成后，`build/arm/` 中通常包含：

```text
framework.elf   # 调试与烧录主文件
framework.hex   # Intel HEX 固件
framework.bin   # 二进制固件
framework.map   # 链接映射文件，可分析 Flash/RAM 占用
framework.dot   # CMake 目标关系图，graphviz 开启时生成
```

## 烧录

使用项目脚本：

```bash
bash scripts/flash.bash
```

或手动使用 pyOCD：

```bash
pyocd flash build/arm/framework.elf --config pyocd.yaml
pyocd reset --config pyocd.yaml
```

## 预览文档

```bash
bash scripts/serve_docs.sh
```

静态构建：

```bash
bash scripts/serve_docs.sh --build
```
