# 07 — SDL2 VM Simulator

## Purpose

Run full application logic on PC for faster development. Under current config, typical iteration cycle: ~42s (hardware flash) → ~19s (VM), roughly 3× iteration density improvement (measured; actual times depend on binary size and host specs).

## Architecture

```
ARM:  APP → HAL(ARM) → BSP(ARM) → DriverLib → MSPM0
VM:   APP → HAL(VM)  → BSP(VM)  → SDL2 → POSIX → x86
```

APP layer code is designed to be identical across platforms. HAL/BSP each have one ARM implementation and one VM stub implementation. Swapped via CMake `BUILD_PLATFORM=VM`.

## How Code Reuse Works

1. APP does not depend on DriverLib — enforced at compile time (#include does not exist)
2. HAL/BSP headers are identical, implementations differ — CMake `INTERFACE` library swaps .c files
3. FreeRTOS API → POSIX stubs — `xTaskCreate → pthread_create`, `xSemaphoreTake → pthread_mutex_lock`
4. Same init sequence — `main_vm.c` calls the same `Bsp_Init/Hal_Init/App_Init` as `main.c`

## Key Components

| Component | ARM | VM |
| --- | --- | --- |
| Display | ST7789 via Soft SPI | SDL2 texture, 2× upscale |
| Input | ADC Joystick + GPIO Button | WASD/Arrow + Space |
| Audio | PWM Buzzer | Nop / console print |
| Flash | W25Q32 via Hard SPI | stub: read returns 0xFF, writes ignored |
| Time | SysTick | SDL_GetTicks |
| RTOS | FreeRTOS kernel | POSIX threads + condvar |

## Controls

```
WASD/Arrow → Joystick
Space → Button (short=confirm, long=back)
ESC → Quit
```

## Limitations

- Does not simulate SPI timing / ADC noise / Flash wear
- No hard real-time guarantees (best-effort pthread)
- Hardware-related bugs still require testing on real device

## Build & Run

```bash
# 1. Confirm there is a target with name: vm in the build: list in config/config.yaml
# 2. Build (--target matches the name field, not platform) and run
python3 scripts/cc.py --target vm
./build/vm/framework_vm
```

## ADR

See [adr/architecture_decisions.md §5](adr/architecture_decisions.md#5-sdl2-vm-simulator).
