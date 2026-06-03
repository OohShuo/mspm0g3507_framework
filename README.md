# MSP430G3507 Framework

一个用于 MSP430G3507 微控制器的嵌入式框架，采用分层架构设计，集成了 FreeRTOS 实时操作系统和 TI DriverLib。

- [MSP430G3507 Framework](#msp430g3507-framework)
  - [项目概述](#项目概述)
  - [项目组织结构](#项目组织结构)
  - [架构分层关系](#架构分层关系)
    - [各层职责](#各层职责)
  - [编译要求](#编译要求)
    - [必需工具](#必需工具)
    - [工具安装](#工具安装)
  - [编译指南](#编译指南)
    - [1. 基本编译流程](#1-基本编译流程)
    - [2. 使用编译脚本](#2-使用编译脚本)
    - [3. 系统配置生成](#3-系统配置生成)
  - [构建输出结构](#构建输出结构)
  - [编译输出示例](#编译输出示例)
  - [代码风格](#代码风格)
  - [参考资源](#参考资源)

## 项目概述

本项目是一个专为 TI MSP430G3507（Arm Cortex-M0+）微控制器优化的嵌入式开发框架，提供了完整的系统级支持，包括硬件抽象层、板级支持包和应用层接口。

## 项目组织结构

``` plain
framework/
├── cmake/                     # CMake构建系统配置
│
├── src/                       # 源代码主目录
│   ├── main.c                 # 系统入口点
│   ├── app/                   # 应用层
│   │   ├── app.c/h            # 应用程序主体
│   │   ├── input/             # 输入处理模块
│   │   ├── .../               # 其他模块
│   │   └── CMakeLists.txt
│   ├── bsp/                   # 板级支持包 (Board Support Package)
│   │   ├── bsp.c/h            # BSP核心接口
│   │   ├── gpio/              # GPIO驱动模块
│   │   ├── .../               # 其他模块
│   │   └── CMakeLists.txt
│   ├── hal/                   # 硬件抽象层 (Hardware Abstraction Layer)
│   │   ├── hal.c/h            # HAL核心接口
│   │   ├── led_simple/        # LED驱动模块
│   │   ├── .../               # 其他模块
│   │   └── CMakeLists.txt
│   ├── lib/                   # 通用库
│   │   ├── vector/            # 动态数组
│   │   ├── .../               # 其他模块
│   │   └── CMakeLists.txt
│   └── syscfg/                   # 系统配置自动生成文件
│       ├── ti_msp_dl_config.c/h  # TI配置文件
│       └── device.opt            # 设备选项配置
│
├── ti_device/               # TI设备支持库
│   ├── cmsis/               # CMSIS核心接口
│   ├── freertos/            # FreeRTOS 实时操作系统
│   └── ti/                  # TI 官方库
│
├── config/                  # 配置文件目录
│   ├── framework.syscfg     # SysConfig项目配置文件
│   ├── board_config.h       # 板级配置头文件
│   └── FreeRTOSConfig.h     # FreeRTOS配置文件
│
├── tools/                   # TI工具链和资源
│   ├── gcc-arm-none-eabi/   # ARM GCC工具链
│   ├── sysconfig/           # TI SysConfig工具
│   ├── ti/
│   └── product.json         # 工具产品配置
│
├── scripts/                 # 构建和辅助脚本
│
└─── CMakeLists.txt          # 主CMake配置文件
```

## 架构分层关系

``` mermaid
graph TD
    app[app]
    bsp[bsp]
    lib[lib]
    ti_device[ti_device]
    driverlib.a["driverlib.a(TI官方驱动库)"]
    c[c]
    gcc[gcc]
    m[m]
    nosys[nosys]
    hal[hal]
    framework_elf["framework.elf"]

    bsp --> lib
    ti_device --> driverlib.a
    ti_device --> c
    ti_device --> gcc
    ti_device --> m
    ti_device --> nosys
    bsp --> ti_device
    app --> bsp
    hal --> bsp
    hal --> lib
    app --> hal
    app --> lib
    
    framework_elf -.-> app
    framework_elf -.-> bsp
    framework_elf -.-> hal
    framework_elf -.-> lib
    framework_elf -.-> ti_device
```

### 各层职责

| 层级 | 目录 | 职责 |
| ---- | ---- | ---- |
| **应用层** | `src/app/` | 业务逻辑实现，调用HAL接口 |
| **库层** | `src/lib/` | 通用数据结构、工具函数 |
| **HAL** | `src/hal/` | 硬件驱动抽象，屏蔽硬件细节 |
| **BSP** | `src/bsp/` | 板级支持，初始化、IO配置 |
| **设备层** | `ti_device/` | 芯片级支持、RTOS、DriverLib |

## 编译要求

### 必需工具

| 工具 | 用途 |
| ---- | ---- |
| **CMake** | 构建系统 |
| **ARM GCC** | ARM交叉编译工具链（已包含在项目中） |
| **TI SysConfig** | GUI配置系统参数、时钟树、外设 |
| **Git** | 版本控制 |
| **Ninja** | 构建系统（CMake的替代后端） |
| **Clang Format** | 代码格式化 |

### 工具安装

- 前往 [ARM GCC 官网](https://developer.arm.com/downloads/-/gnu-rm) 安装对应操作系统的 ARM GCC 工具链，解压后将以下复制到 `tools/gcc-arm-none-eabi/` 目录：

  ``` plain
  tools/gcc-arm-none-eabi/
  ├── arm-none-eabi/
  ├── bin/
  ├── lib/
  └── share/
  ```

- 安装 CMake， Git 和 Ninja

  Ubuntu 系统：

  ```bash
  sudo apt install cmake git ninja-build
  ```

  Windows 系统：

  - CMake: [CMake Download](https://cmake.org/download/)
  - Git: [Git Download](https://git-scm.com/download/win)
  - Ninja: [Ninja Download](https://github.com/ninja-build/ninja/releases)

- TI SysConfig: [TI SysConfig Download](https://www.ti.com.cn/tool/cn/SYSCONFIG)
  
  下载后将以下文件复制到 `tools/sysconfig/` 目录：

  ``` plain
  tools/sysconfig/
  ├── app
  ├── dist
  ├── nodejs
  ├── nw
  ├── spice64
  ├── sysconfig_cli.sh/bat
  └── sysconfig_gui.sh/bat
  ```

- clangd (可选)

  [GitHub Release](https://github.com/clangd/clangd/releases/tag/21.1.8)

## 编译指南

### 1. 基本编译流程

```bash
# 进入项目目录
cd /path/to/framework

# 创建构建目录
mkdir -p build
cd build

# 配置构建（使用CMake）
cmake ..

# 编译
make -j4

# 输出文件
# - framework.elf      : ELF可执行文件
# - framework.hex      : Intel HEX格式固件
# - framework.bin      : 二进制固件
# - framework.map      : 链接地图文件
```

### 2. 使用编译脚本

项目提供了便利的编译脚本：

```bash
# 完整编译流程（使用 cm.bash 脚本）
./scripts/cm.bash

# 清理构建输出
./scripts/clear.bash
```

### 3. 系统配置生成

如需修改系统配置（时钟、外设等）：

```bash
# 打开SysConfig GUI进行配置
./scripts/open_syscfg_gui.bash

# 或手动生成SysConfig文件
./scripts/gen_syscfg_files.bash
```

## 构建输出结构

``` plain
build/
├── framework.elf           # 主可执行文件
├── framework.hex           # HEX格式固件
├── framework.bin           # 二进制固件
├── framework.map           # 链接地图
├── CMakeFiles/             # CMake中间文件
├── cmake_install.cmake
└── Makefile                # 生成的Makefile
```

## 编译输出示例

``` plain
-- Build files have been written to: /home/shuo/ti/framework/build
[65/65] Linking C executable framework.elf
   text    data     bss     dec     hex filename
  13920     112    8696   22728    58c8 /home/shuo/ti/framework/build/framework.elf
```

## 代码风格

项目使用 **Clang Format** 保持一致的代码风格：

```bash
# 格式化所有源文件
clang-format -i src/**/*.c src/**/*.h

# 检查格式
clang-format --dry-run src/**/*.c
```

格式化配置：`.clang-format`

## 参考资源

- [CMake Documentation](https://cmake.org/cmake/help/latest/)
- [ARM GCC Embedded Toolchain](https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain)
- [TI MSP430G3507 Datasheet](https://www.ti.com/)
- [FreeRTOS Documentation](https://www.freertos.org/Documentation/161204_FreeRTOS_Reference_Manual_v9.0.0.pdf)
- [CMSIS Documentation](https://arm-software.github.io/CMSIS_5/General/html/index.html)
