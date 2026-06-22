# MSPM0G3507 Framework

<p align="center">
  <strong>嵌入式游戏机开发框架 · Embedded Game Console Framework</strong><br>
  <small>APP → HAL → BSP → DriverLib 分层架构 · ARM 固件 + x86 SDL2 VM 双平台</small>
</p>

---

## 概览 · Overview

**MSPM0G3507 Framework** 是一个面向 **TI MSPM0G3507** 的嵌入式应用开发框架。项目以小游戏控制台为主要应用场景，覆盖输入、显示、音频、振动、外部 Flash、文件系统、RTOS 任务和 PC 端仿真等模块。

核心目标是：**业务代码尽量写在 APP 层，通过 HAL/BSP 隔离硬件差异，使同一套游戏和 UI 逻辑可以在 ARM 硬件与 PC VM 上复用调试。**

| 芯片 · SoC | 内核 · Core | 主频 · Freq | Flash | SRAM |
| :---: | :---: | :---: | :---: | :---: |
| MSPM0G3507 | ARM Cortex-M0+ | 80 MHz | 128 KB | 32 KB |

---

## 架构 · Architecture

``` mermaid
graph TD
    APP["<b>APP</b><br/>Game Console · Games · Storage · Flash Mgr"]
    HAL["<b>HAL</b><br/>Button · Joystick · ST7789 · W25Q32 · Buzzer · VibMotor"]
    BSP["<b>BSP</b><br/>GPIO · PWM · ADC · SPI · UART · Time"]
    DL["<b>DriverLib / SysConfig</b><br/>TI Register-Level API"]
    MCU["<b>MSPM0G3507</b>"]

    MW["FreeRTOS · LittleFS · LVGL(optional) · RTT(optional)"]
    VM["SDL2 VM<br/>Display · Input · Audio · Vibration Stub"]

    APP --> HAL
    HAL --> BSP
    BSP --> DL
    DL --> MCU

    MW -.-> APP
    MW -.-> HAL
    VM -.-> BSP
```

依赖方向保持单向：`APP` 调用 `HAL`，`HAL` 调用 `BSP`，`BSP` 调用 TI DriverLib / SysConfig。VM 通过模拟 BSP/HAL 行为，让应用层逻辑可以在 PC 上快速验证。

---

## 特性 · Features

<div class="grid cards" markdown>

-   :material-layers-outline:{ .lg .middle } **分层架构**

    APP / HAL / BSP / DriverLib 单向依赖，方便移植、裁剪和测试。

-   :material-chip:{ .lg .middle } **FreeRTOS 任务模型**

    支持任务、队列、信号量、互斥锁和 heap_4，适合组织输入扫描、游戏循环和硬件反馈。

-   :material-gamepad-variant:{ .lg .middle } **游戏控制台**

    内置菜单、游戏信息页、小游戏、统一输入接口、暂停界面、分数和屏保逻辑。

-   :material-monitor-dashboard:{ .lg .middle } **LCD 图形显示**

    面向 ST7789 TFT LCD，支持游戏 UI、图片资源和像素级绘制。

-   :material-harddisk:{ .lg .middle } **外部 Flash 与 LittleFS**

    W25Q32 SPI Flash + LittleFS，用于资源、分数和运行时文件管理。

-   :material-desktop-tower:{ .lg .middle } **x86 SDL2 VM**

    在 Ubuntu/PC 上模拟显示、键盘输入、蜂鸣器/振动等接口，便于快速调试游戏逻辑。

-   :material-hammer-wrench:{ .lg .middle } **YAML 驱动构建**

    `config/config.yaml` 描述 ARM / VM target 和功能开关，`scripts/cc.py` 统一调用 CMake。

-   :material-console:{ .lg .middle } **工具脚本精简**

    `scripts/` 只保留构建、烧录、SysConfig、串口、资源生成和文档预览等必要工具。

</div>

---

## 快速开始 · Quick Start

=== "1. 安装依赖"

    Ubuntu 示例：

    ```bash
    sudo apt update
    sudo apt install -y python3 python3-yaml cmake ninja-build libsdl2-dev
    ```

    文档站点依赖：

    ```bash
    python3 -m pip install -r requirements-docs.txt
    ```

=== "2. 配置 target"

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

=== "3. 构建"

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

=== "4. 运行 VM"

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

=== "5. 烧录 ARM"

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
| 快速构建和运行 | [快速开始](quick_start.md) |
| 理解整体结构 | [项目架构](architecture.md) |
| 查看 FreeRTOS 任务设计 | [FreeRTOS 设计](freertos_design.md) |
| 理解 APP / HAL / BSP / LIB | [模块设计](modules.md) |
| 使用 PC 仿真环境 | [VM 仿真器](vm.md) |
| 理解 CMake 和 target 配置 | [构建系统](build_system.md) |
| 了解 scripts 工具目录 | [脚本说明](scripts.md) |
| 新增模块、游戏或驱动 | [开发者指南](developer_guide.md) |

---

## 脚本目录 · Scripts

清理后的 `scripts/` 目录只保留常用入口和必要工具：

```text
scripts/
├── README.md                     # 脚本目录索引
├── cc.py                         # YAML 驱动构建入口
├── cm.bash                       # Linux/macOS 构建包装
├── cm.cmd                        # Windows 构建包装
├── clear.bash                    # 删除 build/
├── flash.bash                    # pyOCD 烧录
├── gen_syscfg_files.bash         # SysConfig CLI 生成文件
├── open_syscfg_gui.bash          # 打开 SysConfig GUI
├── install_sysconfig.bash        # 安装 SysConfig，不包含 .run 安装包
├── flash_manager.py              # 外部 Flash / LittleFS 串口管理
├── com_uart_test.py              # UART 通信测试
├── generate_air_battle_assets.py # 飞机大战资源生成
├── generate_info_images.py       # Info 页面图片生成
├── serve_docs.sh                 # MkDocs 预览/构建
├── assets/
│   └── LVGLImage.py              # LVGL 图片转换工具，低频使用
└── experimental/
    └── slip_send.py              # SLIP/7D7E 协议实验脚本
```

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
└── build/               # 构建产物，不提交
```

---
