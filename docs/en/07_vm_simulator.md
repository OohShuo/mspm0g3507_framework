# 07 — SDL2 VM Simulator

## Purpose

PC 上运行完整应用逻辑，加速开发。迭代周期：42s（硬件）→ 19s（VM），约 3× 迭代密度提升。

## Architecture

```
ARM:  APP → HAL(ARM) → BSP(ARM) → DriverLib → MSPM0
VM:   APP → HAL(VM)  → BSP(VM)  → SDL2 → POSIX → x86
```

APP 层代码相同。HAL/BSP 各一份 ARM 实现和一份 VM 桩实现。CMake `BUILD_PLATFORM=VM` 时替换。

## How Code Reuse Works

1. APP 不依赖 DriverLib — 编译期保证 (#include 不存在)
2. HAL/BSP 头文件相同，实现不同 — CMake `INTERFACE` 库替换 .c 文件
3. FreeRTOS API → POSIX 桩 — `xTaskCreate → pthread_create`, `xSemaphoreTake → pthread_mutex_lock`
4. 相同 init 序列 — `main_vm.c` 调用与 `main.c` 相同的 `Bsp_Init/Hal_Init/App_Init`

## Key Components

| Component | ARM | VM |
| --- | --- | --- |
| Display | ST7789 via Soft SPI | SDL2 texture, 2× upscale |
| Input | ADC Joystick + GPIO Button | WASD/Arrow + Space |
| Audio | PWM Buzzer | Nop / console print |
| Flash | W25Q32 via Hard SPI | RAM-backed |
| Time | SysTick | SDL_GetTicks |
| RTOS | FreeRTOS kernel | POSIX threads + condvar |

## Controls

```
WASD/Arrow → Joystick
Space → Button (short=confirm, long=back)
ESC → Quit
```

## Limitations

- 不模拟 SPI 时序 / ADC 噪声 / Flash 磨损
- 不提供硬实时保证（best-effort pthread）
- 硬件相关 bug 仍需真机测试

## Build & Run

```bash
# 1. 在 config/config.yaml 的 build: 列表中确认有 name: vm 的 target
# 2. 构建（--target 匹配 name 字段，非 platform）并运行
python3 scripts/cc.py --target vm
./build/vm/framework_vm
```

## ADR

详见 [adr/architecture_decisions.md §5](adr/architecture_decisions.md#5-sdl2-vm-simulator).
