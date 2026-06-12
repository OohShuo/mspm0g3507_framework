# Build — 构建系统

## 概述

基于 CMake + Ninja 的交叉编译构建系统，目标平台 MSPM0G3507（ARM Cortex-M0+ / armv6-m / softfloat）。

## 工具链

| 工具 | 路径 |
| --- | --- |
| ARM GCC | `tools/gcc-arm-none-eabi/` |
| TI SysConfig | `tools/sysconfig/` |
| TI DriverLib | `ti_device/ti/driverlib/lib/gcc/m0p/mspm0g1x0x_g3x0x/driverlib.a` |

## CMake 结构

``` plain
CMakeLists.txt              # 顶层：项目声明、输出目标 framework.elf
├── cmake/
│   ├── toolchain.cmake     # ARM GCC 交叉编译工具链定义
│   ├── tools.cmake         # SysConfig 目标生成
│   ├── lvgl_config.cmake   # LVGL 构建参数（字体、色深）
│   └── utils.cmake         # 辅助函数（get_subdirs_and_self、彩色输出）
├── lib/
│   └── */CMakeLists.txt    # 各库独立构建（静态库）
├── src/
│   ├── app/CMakeLists.txt  # libapp.a
│   ├── bsp/CMakeLists.txt  # libbsp.a
│   ├── hal/CMakeLists.txt  # libhal.a
│   ├── test/CMakeLists.txt # libtest.a
│   └── syscall/CMakeLists.txt # libsyscall.a（newlib retarget）
├── ti_device/CMakeLists.txt # libti.a（DriverLib + 启动文件 + 链接脚本）
```

链接顺序：`app → hal → bsp → test → syscall → lib → ti_device`

## 构建脚本

| 脚本 | 功能 |
| --- | --- |
| `scripts/cm.bash` | 一键 CMake + Ninja 构建 |
| `scripts/clear.bash` | 清理 `build/` 目录 |
| `scripts/gen_syscfg_files.bash` | 命令行生成 SysConfig |
| `scripts/open_syscfg_gui.bash` | 打开 SysConfig GUI |
| `scripts/install_sysconfig.bash` | 一键安装 SysConfig 到 tools/ |

## 构建输出

| 文件 | 说明 |
| --- | --- |
| `framework.elf` | ELF 可执行文件（含调试信息） |
| `framework.hex` | Intel HEX 格式固件 |
| `framework.bin` | 二进制固件 |
| `framework.map` | 链接地图 |
| `framework.dot` | CMake 依赖关系图（需 `graphviz: ON`） |

## 内存布局

链接脚本 `mspm0g3507.lds` 定义：

| 区域 | 大小 | 用途 |
| --- | --- | --- |
| FLASH | 128 KB | 代码 + 只读数据 |
| SRAM | 32 KB | 数据 + BSS + 堆 + FreeRTOS 栈 |
| BCR_CONFIG | 255 B | BCR 配置区 |
| BSL_CONFIG | 128 B | BSL 配置区 |
