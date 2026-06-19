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

- 2 layers (APP+DriverLib): VM impossible, porting requires rewriting everything
- 3 layers (merge HAL+BSP): pin mapping coupled with driver logic, changing MCU requires rewriting all HAL
- 5 layers (add Platform Abstraction): more indirection, unacceptable for Cortex-M0+

4 layers is the minimum set to satisfy "VM parity + MCU portability". See [adr/architecture_decisions.md §1](adr/architecture_decisions.md#1-layered-architecture).

### Why APP Must Never Depend on DriverLib

This is not a code-cleanliness issue — it's the enabling condition for the VM. APP includes DriverLib → VM build fails → someone adds `#ifdef` → VM becomes unreliable. Enforced by the compiler, non-negotiable.

**The only exception**: `Bsp_Get_Tick_Ms()` (known technical debt, should move to the syscall layer).

### Why HAL (Not Direct BSP from APP)

- Multi-instance: two LEDs = two HAL objects, not two sets of globals
- State encapsulation: Button debounce, Buzzer note sequences, Joystick calibration
- Test isolation: can create a Button object standalone to test debounce logic
- VM swapping: HAL interface unchanged, ARM/VM implementations swap freely

### Why SDL2 VM (Not QEMU/Renode)

- QEMU has no MSPM0G3507 model
- Renode requires writing C# peripheral models
- SDL2 VM only needs stubs for HAL/BSP (20-50 lines each)
- Iteration data: hardware ~42s/cycle vs VM ~19s/cycle, roughly 3× iteration density

See [adr/architecture_decisions.md §5](adr/architecture_decisions.md#5-sdl2-vm-simulator).

### Why FreeRTOS

Mature Cortex-M0+ port (handles no-CLZ). ~12KB Flash. heap_4 best-fit. Widely documented. vs RTX (license), Zephyr (too large), bare-metal (reinvent scheduler). [adr §2](adr/architecture_decisions.md#2-freertos).

### Why LittleFS

COW → survives power loss. Dynamic wear leveling → no fixed superblock location. Static RAM → 528B fixed. vs FatFS (no wear leveling, FAT corruption), SPIFFS (unmaintained, O(n) directory). [adr §3](adr/architecture_decisions.md#3-littlefs).

### Why LVGL Optional

Games use direct framebuffer rendering (effectively 0 RAM overhead). LVGL widget system suits settings / file manager. Both coexist; avoids forcing the cost. [adr §4](adr/architecture_decisions.md#4-lvgl).

## Forbidden Dependencies

```
APP ──✕──► DriverLib    VM parity
HAL ──✕──► DriverLib    BSP sole consumer
BSP ──✕──► APP/HAL      Layer inversion
APP ──⚠──► BSP          Tolerated: Bsp_Get_Tick_Ms() only
```

## Rules Contributors Must Follow

1. APP/HAL must never include DriverLib
2. BSP functions ≤30 lines (thin wrappers)
3. New modules must have a VM stub
4. Configuration comes from config files, not hardcoded
5. `#if MACRO` not `#ifdef` (macros always defined as 0/1)
6. Naming: public `PascalCase`, static `snake_case`, macro `UPPER_CASE`

## Accepted Tradeoffs

| Tradeoff | Accepted Because |
| --- | --- |
| 4-layer indirection vs direct register writes | Enable VM + MCU portability |
| Static allocation (no Destroy) vs dynamic lifetime | Eliminate use-after-free; MCU RAM is limited |
| Compile-time config vs runtime config | Negligible RAM/CPU cost |
| Soft SPI for LCD vs hardware SPI | Save one SPI peripheral for W25Q32 |
| Direct framebuffer vs LVGL | Save ~30KB RAM |
| C-only vs C++ | Smaller binary, no vtable/RTTI overhead |
