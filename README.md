# MSPM0G3507 Framework

<p align="center">
  <strong>嵌入式游戏机开发框架 · Embedded Game Console Framework</strong><br>
  <small>ARM 固件 + x86 SDL2 VM 双平台</small>
</p>

---

> 建议在 Linux 下使用，跨平台不保证能够顺利运行，Windows 环境配置详见 [Windows 环境配置](docs/win_env.md)。

## 概览 · Overview

**MSPM0G3507 Framework** 是一个面向 TI MSPM0G3507 (**~~电工实习~~**) 的嵌入式应用开发框架。项目以小游戏控制台为主要应用场景，覆盖输入、显示、音频、振动、外部 Flash、文件系统、RTOS 任务和 PC 端仿真等模块。

| 芯片 · SoC | 内核 · Core | 主频 · Freq | Flash | SRAM |
| :---: | :---: | :---: | :---: | :---: |
| MSPM0G3507 | ARM Cortex-M0+ | 80 MHz | 128 KB | 32 KB |

---

## 架构 · Architecture

``` mermaid
graph TD
    subgraph ARM
        direction LR

        M["<b>HAL -> BSP -> DL</b>"] --> MC["<b>MCU</b><br/>MSPM0G3507"]
    end

    APP["<b>APP</b><br/>Game Console · Games · Storage · Flash Mgr"]
    

    MW["<b>Middleware</b><br/>FreeRTOS · LittleFS · LVGL(optional) · RTT(optional) · Local Lib"]
    VM["<b>SDL2 VM</b><br/>Display · Input · Audio · Vibration Stub"]

    MAIN["<b>Main</b><br/>Application Entry"]

    MAIN --> APP

    APP --"platform = arm"--> ARM
    APP --"platform = vm"--> VM

```

业务代码写在 APP 层，通过模块依赖隔离硬件差异，使同一套游戏和 UI 逻辑可以在 ARM 硬件与 PC VM 上复用调试。

---

## 特性 · Features

- **分层架构** — APP / HAL / BSP / DriverLib 单向依赖，方便移植、裁剪和测试。
- **FreeRTOS 任务模型** — 支持任务、队列、信号量、互斥锁和 heap_4，适合组织输入扫描、游戏循环和硬件反馈。
- **游戏控制台** — 内置菜单、游戏信息页、小游戏、统一输入接口、暂停界面、分数和屏保逻辑。
- **LCD 图形显示** — 面向 ST7789 TFT LCD，支持游戏 UI、图片资源和像素级绘制。
- **外部 Flash 与 LittleFS** — W25Q32 SPI Flash + LittleFS，用于资源、分数和运行时文件管理。PC 端通过 [Flash Manager](docs/flash_mgr.md) 管理文件。
- **x86 SDL2 VM** — 在 Ubuntu/PC 上模拟显示、键盘输入、蜂鸣器/振动等接口，便于快速调试游戏逻辑。
- **YAML 驱动构建** — `config/config.yaml` 描述 ARM / VM target 和功能开关，`scripts/cc.py` 统一调用 CMake。
- **工具脚本** — `scripts` 内有构建、烧录、SysConfig、串口、资源生成、图片/视频转换和文档预览等必要工具。

---

## 快速开始 · Quick Start

### 1. 安装依赖

Ubuntu 示例：

```bash
sudo apt update
sudo apt install -y python3 python3-yaml cmake ninja-build libsdl2-dev
```

文档站点依赖：

```bash
python3 -m pip install -r requirements-docs.txt
```

下载 [GNU Arm Embedded Toolchain](https://developer.arm.com/downloads/-/gnu-rm)，解压到 `tools/gcc-arm-none-eabi` 目录；VM 目标使用系统 GCC/Clang 与 SDL2。

下载 [TI SysConfig 工具](https://www.ti.com/tool/SYSCONFIG?utm_source=google&utm_medium=cpc&utm_campaign=epd-der-null-58700007779115364_sysconfig_rsa-cpc-evm-google-ww_en_int&utm_content=sysconfig&ds_k=sysconfig&gclsrc=aw.ds&gad_source=1&gad_campaignid=12788839648&gbraid=0AAAAAC068F0mxDINEjN5e5Md3f4ZsSyBs&gclid=CjwKCAjwuuPRBhAnEiwA2Ji8eiK_ixXpEXuhgDtRp0YhwTWHAC6KOf8EZ79ZcwkbVHbUfiH5GBbcehoCNecQAvD_BwE)，解压到 `tools/sysconfig` 目录。

### 2. 配置 target

编辑 `config/config.yaml`，选择构建目标和功能开关：

```yaml
build:
  - name: arm
    platform: ARM
    build_type: MinSizeRel
    FRAMEWORK_USE_FREERTOS: ON
    FRAMEWORK_USE_LVGL: OFF
    FRAMEWORK_USE_LFS: ON
    FRAMEWORK_USE_RTT: OFF

  - name: vm
    platform: VM
    build_type: Release
    FRAMEWORK_USE_FREERTOS: ON
    FRAMEWORK_USE_LFS: ON
```

`name` 决定 `build/<name>/` 目录；`platform` 决定 ARM 固件或 VM 仿真；`FRAMEWORK_USE_*` 控制中间件和模块裁剪。

### 3. 构建

```bash
# 构建 config.yaml 中的所有 target
python3 scripts/cc.py

# 仅构建 ARM 固件
python3 scripts/cc.py --target arm

# 仅构建 VM 仿真器
python3 scripts/cc.py --target vm

# Linux/macOS 包装入口
bash scripts/cm.bash --target vm
```

不建议直接手写 `cmake` 命令绕过 `cc.py`，否则 `config/config.yaml` 中的功能开关不会自动传入。

### 4. 运行 VM

```bash
./build/vm/framework_vm
```

VM 默认按键：

| 键盘 | 含义 |
| --- | --- |
| 方向键 | 摇杆方向 |
| `S` | A |
| `D` | B |
| `W` | X |
| `A` | Y |
| `Space` | START / 摇杆按键兼容输入 |
| `Esc` | 退出 VM |

### 5. 烧录 ARM

```bash
python3 scripts/cc.py --target arm
bash scripts/flash.bash
```

常见输出位于：

```text
build/arm/framework.elf
build/arm/framework.hex
build/arm/framework.bin
build/arm/framework.map
```

---

## 文档导航 · Doc Map

| 你想做什么 · What to do | 文档 |
| :--- | :--- |
| 快速构建和运行 | [快速开始](docs/quick_start.md) |
| 理解整体结构 | [项目架构](docs/architecture.md) |
| 查看 FreeRTOS 任务设计 | [FreeRTOS 设计](docs/freertos_design.md) |
| 理解 APP / HAL / BSP / LIB | [模块设计](docs/modules.md) |
| 使用 PC 仿真环境 | [VM 仿真器](docs/vm.md) |
| 理解 CMake 和 target 配置 | [构建系统](docs/build_system.md) |
| 了解 scripts 工具目录 | [脚本说明](docs/scripts.md) |
| 新增模块、游戏或驱动 | [开发者指南](docs/developer_guide.md) |
| Windows 环境配置 | [Windows 环境配置](docs/win_env.md) |

---

## 项目结构 · Project Layout

```text
framework/
├── assets/              # 图片、字体、游戏资源等
├── cmake/               # 工具链、构建函数、平台配置
├── config/              # config.yaml、board_config.h、FreeRTOSConfig、SysConfig 工程
├── docs/                # MkDocs 文档
├── lib/                 # FreeRTOS、LittleFS、LVGL、RTT、local_lib
├── scripts/             # 构建、烧录、SysConfig、资源和串口工具
├── src/
│   ├── app/             # APP：游戏控制台、小游戏、存储管理
│   ├── bsp/             # BSP：GPIO、PWM、ADC、SPI、UART、Time
│   ├── hal/             # HAL：Button、Joystick、ST7789、W25Q32、Buzzer、VibMotor
│   ├── platform/        # ARM / VM 平台入口适配
│   ├── syscall/         # retarget、RTT log 等系统调用适配
│   ├── test/            # 可开关的外设和模块测试任务
│   └── vm/              # SDL2 虚拟硬件层
├── ti_device/           # TI CMSIS、DriverLib、链接脚本
├── tools/               # ARM GCC、TI SysConfig、pack、SVD 等本地工具
└── build/               # 构建产物
```

---
