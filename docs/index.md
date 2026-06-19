# MSPM0G3507 Framework

<p align="center">
  <strong>嵌入式应用开发框架 · Embedded Application Framework</strong><br>
  <small>APP → HAL → BSP → DriverLib 分层架构 · 双平台 ARM + x86 SDL2 VM</small>
</p>

---

## 概览 · Overview

基于 **TI MSPM0G3507**（ARM Cortex-M0+ @ 80 MHz）的完整嵌入式软件栈——从寄存器级驱动到游戏控制台，覆盖全链路。应用层代码在 ARM 硬件和 PC 虚拟机上 **100% 复用**，无需修改一行业务逻辑。

| 芯片 · SoC | 内核 · Core | 主频 · Freq | Flash | SRAM |
| :---: | :---: | :---: | :---: | :---: |
| MSPM0G3507 | ARM Cortex-M0+ | 80 MHz | 128 KB | 32 KB |

---

## 架构 · Architecture

``` mermaid
graph TD
    APP["<b>APP</b><br/>Game Console · Storage · Flash Mgr"]
    HAL["<b>HAL</b><br/>ST7789 · W25Q32 · Joystick · Buzzer"]
    BSP["<b>BSP</b><br/>GPIO · PWM · ADC · SPI · UART · Time"]
    DL["<b>DriverLib</b><br/>TI Register-Level API"]
    MCU["<b>MSPM0G3507</b>"]

    MW["FreeRTOS · LVGL · LittleFS · RTT"]

    APP --> HAL
    HAL --> BSP
    BSP --> DL
    DL --> MCU
    MW -.-> APP
    MW -.-> HAL
```

单向依赖，上层调用下层，禁止反向。3 层间接调用在 `-Os` 编译下内联为零开销。

---

## 特性 · Features

<div class="grid cards" markdown>

-   :material-chip:{ .lg .middle } **RTOS 内核**

    FreeRTOS v11.x — 任务、队列、信号量、互斥锁，heap_4 防碎片

-   :material-layers-outline:{ .lg .middle } **分层架构**

    APP → HAL → BSP → DriverLib 单向依赖，编译期模块组合

-   :material-monitor-dashboard:{ .lg .middle } **图形界面**

    LVGL 图形库（实际未启用），ST7789 TFT LCD 320×240 驱动

-   :material-harddisk:{ .lg .middle } **文件系统**

    LittleFS + W25Q32 SPI Flash，磨损均衡 + 掉电保护

-   :material-bug-outline:{ .lg .middle } **调试日志**

    SEGGER RTT — 高速调试输出，不占用 UART，无需额外硬件

-   :material-desktop-tower:{ .lg .middle } **VM 仿真器**

    SDL2 虚拟机 — PC 上运行完整应用，游戏/UI可调试，未接入存储/音乐接口

-   :material-gamepad-variant:{ .lg .middle } **内置游戏**

    多款小游戏 + 菜单系统 + 高分榜 + 屏保动画

-   :material-translate:{ .lg .middle } **双语文档**

    完整中英文文档，架构决策记录（ADR），开发者指南

</div>

---

## 快速开始 · Quick Start

=== "1. 配置"

    编辑 `config/config.yaml`，选择目标平台和功能模块：

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
        # ... 同上 feature flags
    ```

    platform 支持 ARM 和 VM (需安装 SDL2: `sudo apt install libsdl2-dev`)

    禁用模块 → 零 RAM/Flash 占用。编译期组合，无运行时开销。

=== "2. 构建"

    ```bash
    # 构建所有 target（config.yaml 中 build: 列表的每一项）
    python3 scripts/cc.py

    # 仅构建 name=arm 的 target（--target 匹配 name 字段）
    python3 scripts/cc.py --target arm

    # 仅构建 name=vm 的 target
    python3 scripts/cc.py --target vm

    # 或使用 bash 快捷方式
    bash scripts/cm.bash --target vm
    ```

    `cc.py` 读取 `config.yaml`，按 `name` 匹配 target，将 `platform`、`FRAMEWORK_USE_*` 等字段作为 `-D` 传递给 CMake。**直接运行 cmake 会跳过配置**，导致模块开关不生效。

=== "3. 运行 (VM)"

    ```bash
    ./build/vm/framework_vm       # cc.py 构建后产物在 build/<name>/
    ```

=== "4. 烧录 (ARM)"

    ```bash
    # 产物: build/arm/framework.elf / .hex / .bin
    # 通过调试器烧录到 MSPM0G3507
    ```

---

## 文档导航 · Doc Map

| 你想做什么 · What to do | :flag_cn: 中文 | :flag_gb: English |
| :--- | :--- | :--- |
| 理解架构 | [架构总览](zh/01_architecture.md) | [Overview](en/01_architecture.md) |
| 构建项目 | [构建系统](zh/02_build_system.md) | [Build System](en/02_build_system.md) |
| 查找 API | [BSP/HAL/APP](zh/03_bsp_hal_app.md) | [BSP/HAL/APP](en/03_bsp_hal_app.md) |
| 了解中间件 | [中间件](zh/04_middleware.md) | [Middleware](en/04_middleware.md) |
| 了解存储 | [存储系统](zh/05_storage.md) | [Storage](en/05_storage.md) |
| 游戏开发 | [游戏控制台](zh/06_game_console.md) | [Game Console](en/06_game_console.md) |
| 使用 VM | [VM 仿真器](zh/07_vm_simulator.md) | [VM Simulator](en/07_vm_simulator.md) |
| 修改配置 | [配置系统](zh/08_configuration.md) | [Configuration](en/08_configuration.md) |
| 移植 MCU | [移植指南](zh/09_porting.md) | [Porting Guide](en/09_porting.md) |
| 新增模块 | [开发者指南](zh/10_developer_guide.md) | [Developer Guide](en/10_developer_guide.md) |
| 设计哲学 | [设计原则](zh/11_design_principles.md) | [Design Principles](en/11_design_principles.md) |
| 内存布局 | [内存布局](zh/12_memory_layout.md) | [Memory Layout](en/12_memory_layout.md) |
| 架构决策 | [ADR](en/adr/architecture_decisions.md) | [ADR](en/adr/architecture_decisions.md) |

---

## 项目结构 · Project Layout

```
framework/
├── src/
│   ├── app/          # APP — 游戏、存储、Flash 管理
│   ├── hal/          # HAL — ST7789, W25Q32, Joystick, Buzzer 等
│   ├── bsp/          # BSP — GPIO, PWM, ADC, SPI, UART, Time
│   ├── vm/           # SDL2 虚拟机
│   ├── test/         # 测试模块
│   └── syscall/      # retarget / RTT log
├── lib/              # FreeRTOS, LVGL, LittleFS, RTT, local_lib
├── config/           # config.yaml, board_config.h, FreeRTOSConfig.h 等
├── cmake/            # CMake 工具链与函数
├── scripts/          # 构建、烧录、SysConfig、资源脚本
├── docs/             # MkDocs 文档
├── ti_device/        # TI DriverLib / CMSIS / linker script
└── build/            # 构建产物
```

---
