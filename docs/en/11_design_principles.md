# 11 — Design Principles

## Core Philosophy

```
Rigid layers → Predictable changes
Thin BSP → Portable hardware
Object HAL → Testable multi-instance drivers
Descriptor APP → Extensible without core changes
Compile-time config → Zero-cost feature control
VM parity → Fast iteration, guaranteed code reuse
```

## Key Design Decisions

### Why 4 Layers (Not 2, 3, or 5)

- 2 层（APP+DriverLib）：VM 不可能，移植需重写一切
- 3 层（合并 HAL+BSP）：引脚映射与驱动逻辑耦合，换 MCU 需改所有 HAL
- 5 层（加 Platform Abstraction）：更多间接调用，Cortex-M0+ 无法承受

4 层是满足 "VM parity + MCU portability" 的最小集合。详见 [adr/architecture_decisions.md §1](adr/architecture_decisions.md#1-layered-architecture).

### Why APP Must Never Depend on DriverLib

不是代码整洁问题——是 VM 存在的前提条件。APP include DriverLib → VM 编译失败 → 有人加 `#ifdef` → VM 不可靠。由编译器强制执行，不可协商。

**唯一例外**: `Bsp_Get_Tick_Ms()`（已知技术债务，应移到 syscall 层）。

### Why HAL (Not Direct BSP from APP)

- 多实例：两个 LED = 两个 HAL 对象，非两组全局变量
- 状态封装：Button 消抖、Buzzer 音符序列、Joystick 校准
- 测试隔离：可单独创建 Button 对象测试消抖逻辑
- VM swapping：HAL 接口不变，ARM/VM 实现互换

### Why SDL2 VM (Not QEMU/Renode)

- QEMU 无 MSPM0G3507 模型
- Renode 需写 C# 外设模型
- SDL2 VM 只需为 HAL/BSP 写桩（每个 20-50 行）
- 迭代数据：硬件 42s/周期 vs VM 19s/周期，约 3× 迭代密度

详见 [adr/architecture_decisions.md §5](adr/architecture_decisions.md#5-sdl2-vm-simulator).

### Why FreeRTOS

Mature Cortex-M0+ port (handles no-CLZ). ~12KB Flash. heap_4 best-fit. Widely documented. vs RTX (license), Zephyr (too large), bare-metal (reinvent scheduler). [adr §2](adr/architecture_decisions.md#2-freertos).

### Why LittleFS

COW → 掉电不损坏。Dynamic wear leveling → 无固定超块位置。Static RAM → 528B 固定。vs FatFS (no wear leveling, FAT corruption), SPIFFS (unmaintained, O(n) directory). [adr §3](adr/architecture_decisions.md#3-littlefs).

### Why LVGL Optional

游戏用直接 framebuffer 渲染（0 RAM 开销）。LVGL widget 系统适用于设置/文件管理器。同时存在两者，避免强制成本。 [adr §4](adr/architecture_decisions.md#4-lvgl).

## Forbidden Dependencies

```
APP ──✕──► DriverLib    VM parity
HAL ──✕──► DriverLib    BSP sole consumer
BSP ──✕──► APP/HAL      Layer inversion
APP ──⚠──► BSP          Tolerated: Bsp_Get_Tick_Ms() only
```

## Rules Contributors Must Follow

1. APP/HAL 永不 include DriverLib
2. BSP 函数 ≤30 行（薄封装）
3. 新模块必须有 VM stub
4. 配置来自 config 文件，不硬编码
5. `#if MACRO` 不用 `#ifdef`（宏始终定义 0/1）
6. 命名：public `PascalCase`，static `snake_case`，macro `UPPER_CASE`

## Accepted Tradeoffs

| Tradeoff | Accepted Because |
| --- | --- |
| 4 层间接调用 vs 直接写寄存器 | Enable VM + MCU portability |
| 静态分配（无 Destroy） vs 动态生命周期 | 消除 use-after-free；MCU RAM 有限 |
| 编译期配置 vs 运行时配置 | 零 RAM/CPU 成本 |
| Soft SPI for LCD vs 硬件 SPI | 节省一个 SPI 外设给 W25Q32 |
| 直接 framebuffer vs LVGL | 节省 ~30KB RAM |
| C-only vs C++ | 更小二进制，无 vtable/RTTI 开销 |
