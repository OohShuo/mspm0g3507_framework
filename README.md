# MSPM0G3507 Framework

一个用于 TI MSPM0G3507 微控制器的嵌入式框架，采用分层架构设计，集成 FreeRTOS 实时操作系统、LVGL 图形库和 TI DriverLib。

- [MSPM0G3507 Framework](#mspm0g3507-framework)
  - [项目概述](#项目概述)
  - [功能特性](#功能特性)
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

本项目是一个专为 TI MSPM0G3507（Arm Cortex-M0+ / armv6-m / softfloat）微控制器优化的嵌入式开发框架，提供完整的系统级支持，包括硬件抽象层、板级支持包、应用层接口以及 GUI 支持。构建系统基于 CMake + Ninja，并同时支持 Linux 与 Windows 主机开发环境。

## 功能特性

- 分层架构：`app` / `hal` / `bsp` / `lib` / `ti_device` 清晰解耦
- FreeRTOS 多任务调度（GPIO 任务、App 任务、Buzzer 任务）
- LVGL v9.5 图形库集成（独立 `lv_conf.h` 配置）
- BSP 已封装外设：GPIO、PWM、ADC（含 DMA）、时基
- HAL 内置驱动：简易 LED、呼吸 LED、按键、摇杆、蜂鸣器（含 Music Library）
- 通过 TI SysConfig 图形化配置时钟树与外设引脚
- 跨平台构建脚本（`*.bash` + 自动选择 `.sh`/`.bat` SysConfig 后端）

## 项目组织结构

``` plain
framework/
├── cmake/                       # CMake 构建系统配置
│   ├── toolchain.cmake          # ARM GCC 工具链定义
│   ├── tools.cmake              # SysConfig 生成相关
│   ├── lvgl_config.cmake        # LVGL 构建参数
│   └── utils.cmake              # 通用工具宏与彩色输出
│
├── src/                         # 源代码主目录
│   ├── main.c                   # 系统入口点，创建 FreeRTOS 任务
│   ├── it.c                     # 中断服务例程
│   ├── app/                     # 应用层
│   │   ├── app.c/h              # 应用程序主体
│   │   └── CMakeLists.txt
│   ├── bsp/                     # 板级支持包 (Board Support Package)
│   │   ├── bsp.c/h              # BSP 核心接口
│   │   ├── gpio/                # GPIO 驱动模块
│   │   ├── pwm/                 # PWM 驱动模块
│   │   ├── adc/                 # ADC 驱动模块（含 DMA）
│   │   ├── time/                # 时基模块
│   │   └── CMakeLists.txt
│   ├── hal/                     # 硬件抽象层 (Hardware Abstraction Layer)
│   │   ├── hal.c/h              # HAL 核心接口
│   │   ├── led_simple/          # 简易 LED 驱动
│   │   ├── led_breath/          # 呼吸 LED（PWM）
│   │   ├── button/              # 按键驱动
│   │   ├── joystick/            # 摇杆驱动（ADC）
│   │   ├── buzzer/              # 蜂鸣器与 Music Library
│   │   └── CMakeLists.txt
│   └── syscfg/                  # SysConfig 自动生成文件
│       ├── ti_msp_dl_config.c/h # TI 配置文件
│       └── device.opt           # 设备选项配置
│
├── lib/                         # 通用 / 第三方库
│   ├── local_lib/               # 项目内通用库
│   │   └── vector/              # 动态数组
│   ├── freertos/                # FreeRTOS 实时操作系统
│   └── lvgl/                    # LVGL v9.5 图形库
│
├── ti_device/                   # TI 设备支持库
│   ├── cmsis/                   # CMSIS 核心接口
│   └── ti/                      # TI 官方库（DriverLib、启动文件、链接脚本）
│
├── config/                      # 项目级配置文件
│   ├── framework.syscfg         # SysConfig 项目配置文件
│   ├── board_config.h           # 板级外设映射（GPIO/PWM/ADC 索引）
│   ├── FreeRTOSConfig.h         # FreeRTOS 配置
│   └── lv_conf.h                # LVGL 配置
│
├── tools/                       # TI 工具链和资源
│   ├── gcc-arm-none-eabi/       # ARM GCC 工具链
│   ├── sysconfig/               # TI SysConfig 工具
│   ├── ti/                      # TI 设备包
│   └── product.json             # 工具产品配置
│
├── scripts/                     # 构建和辅助脚本
│   ├── cm.bash                  # CMake + Ninja 一键构建
│   ├── clear.bash               # 清理 build/
│   ├── gen_syscfg_files.bash    # 命令行生成 SysConfig
│   ├── open_syscfg_gui.bash     # 启动 SysConfig GUI
│   └── install_sysconfig.bash   # 一键安装 SysConfig
│
└── CMakeLists.txt               # 顶层 CMake 配置
```

## 架构分层关系

``` mermaid
graph TD
    app[app]
    bsp[bsp]
    hal[hal]
    lib[lib]
    local_lib[local_lib]
    freertos[freertos]
    lvgl[lvgl]
    ti_device[ti_device]
    driverlib.a["driverlib.a(TI官方驱动库)"]
    c[c]
    gcc[gcc]
    m[m]
    nosys[nosys]
    framework_elf["framework.elf"]

    lib --> local_lib
    lib --> freertos
    lib --> lvgl
    lvgl --> freertos

    ti_device --> driverlib.a
    ti_device --> c
    ti_device --> gcc
    ti_device --> m
    ti_device --> nosys

    bsp --> lib
    bsp --> ti_device
    hal --> bsp
    hal --> lib
    app --> hal
    app --> bsp
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
| **应用层** | `src/app/` | 业务逻辑实现，调用 HAL 接口 |
| **HAL** | `src/hal/` | 硬件驱动抽象（LED、按键、摇杆、蜂鸣器），屏蔽硬件细节 |
| **BSP** | `src/bsp/` | 板级支持，初始化 GPIO/PWM/ADC/时基 |
| **库层** | `lib/` | FreeRTOS、LVGL、通用数据结构与工具函数 |
| **设备层** | `ti_device/` | 芯片级支持（CMSIS、启动文件、DriverLib、链接脚本） |

## 编译要求

### 必需工具

| 工具 | 用途 |
| ---- | ---- |
| **CMake** | 构建系统（≥ 3.15） |
| **Ninja** | 构建后端（推荐） |
| **ARM GCC** | ARM 交叉编译工具链 |
| **TI SysConfig** | GUI 配置系统参数、时钟树、外设 |
| **Git** | 版本控制 |
| **Clang Format** | 代码格式化（可选） |
| **clangd** | C/C++ Language Server（可选） |

### 工具安装

- 前往 [ARM GCC 官网](https://developer.arm.com/downloads/-/gnu-rm) 下载对应操作系统的 ARM GCC 工具链，解压后将以下文件复制到 `tools/gcc-arm-none-eabi/` 目录：

  ``` plain
  tools/gcc-arm-none-eabi/
  ├── arm-none-eabi/
  ├── bin/
  ├── lib/
  └── share/
  ```

- 安装 CMake、Git 和 Ninja

  Ubuntu 系统：

  ```bash
  sudo apt install cmake git ninja-build
  ```

  Windows 系统：

  - CMake: [CMake Download](https://cmake.org/download/)
  - Git: [Git Download](https://git-scm.com/download/win)
  - Ninja: [Ninja Download](https://github.com/ninja-build/ninja/releases)

- TI SysConfig: [TI SysConfig Download](https://www.ti.com.cn/tool/cn/SYSCONFIG)

  方式一（推荐）：将下载的安装包重命名为 `sysconfig-1.27.1_4634-setup.run` 放至 `scripts/` 后执行：

  ```bash
  ./scripts/install_sysconfig.bash
  ```

  方式二：手动将以下文件复制到 `tools/sysconfig/` 目录：

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

# 配置构建（使用 CMake + Ninja）
cmake -G Ninja ..

# 编译
ninja -j$(nproc)

# 输出文件
# - framework.elf      : ELF 可执行文件
# - framework.hex      : Intel HEX 格式固件
# - framework.bin      : 二进制固件
# - framework.map      : 链接地图文件
```

### 2. 使用编译脚本

项目提供了便利的编译脚本：

```bash
# 完整编译流程（CMake + Ninja）
./scripts/cm.bash

# 清理构建输出
./scripts/clear.bash
```

### 3. 系统配置生成

如需修改系统配置（时钟、外设等）：

```bash
# 打开 SysConfig GUI 进行配置
./scripts/open_syscfg_gui.bash

# 或手动生成 SysConfig 文件（写入 src/syscfg/）
./scripts/gen_syscfg_files.bash
```

构建过程中 `syscfg_gen_target` 也会在 `framework.syscfg` 变化时自动重新生成 `src/syscfg/` 下的 `ti_msp_dl_config.c/h` 与 `device.opt`。

板级外设（GPIO / PWM / ADC 通道索引等）通过 `config/board_config.h` 中的宏定义集中映射，业务代码以 `*_IDX` 形式引用，便于换板。

## 构建输出结构

``` plain
build/
├── framework.elf           # 主可执行文件
├── framework.hex           # HEX 格式固件
├── framework.bin           # 二进制固件
├── framework.map           # 链接地图
├── framework.dot           # 依赖关系图（CMake --graphviz）
├── syscfg_temp/            # SysConfig 生成中间目录
├── CMakeFiles/             # CMake 中间文件
├── cmake_install.cmake
└── build.ninja             # 生成的 Ninja 文件
```

## 编译输出示例

``` plain
-- Build files have been written to: /home/shuo/ti/framework/build
[N/N] Linking C executable framework.elf
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

格式化配置：`.clang-format`；clangd 配置：`.clangd`

## 参考资源

- [TI MSPM0G3507 产品页](https://www.ti.com/product/MSPM0G3507)
- [TI MSPM0 SDK 文档](https://www.ti.com/tool/MSPM0-SDK)
- [TI SysConfig](https://www.ti.com/tool/SYSCONFIG)
- [CMake Documentation](https://cmake.org/cmake/help/latest/)
- [ARM GCC Embedded Toolchain](https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain)
- [FreeRTOS Documentation](https://www.freertos.org/Documentation/RTOS_book.html)
- [LVGL Documentation](https://docs.lvgl.io/)
- [CMSIS Documentation](https://arm-software.github.io/CMSIS_5/General/html/index.html)
