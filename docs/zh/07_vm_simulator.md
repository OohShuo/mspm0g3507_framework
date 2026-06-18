# 07 — SDL2 虚拟机仿真器

## 目的

在 PC 上运行完整应用逻辑，加速开发。迭代周期：42s（硬件）→ 19s（VM），约 3 倍迭代密度提升。

## 架构

```
ARM:  APP → HAL(ARM) → BSP(ARM) → DriverLib → MSPM0
VM:   APP → HAL(VM)  → BSP(VM)  → SDL2 → POSIX → x86
```

APP 层代码相同。HAL/BSP 各一份 ARM 实现和一份 VM 桩实现。CMake `BUILD_PLATFORM=VM` 时替换。

## 代码复用机制

1. APP 不依赖 DriverLib — 编译期保证（`#include` 不存在）
2. HAL/BSP 头文件相同，实现不同 — CMake `INTERFACE` 库替换 .c 文件
3. FreeRTOS API → POSIX 桩 — `xTaskCreate → pthread_create`、`xSemaphoreTake → pthread_mutex_lock`
4. 相同初始化序列 — `main_vm.c` 调用与 `main.c` 相同的 `Bsp_Init/Hal_Init/App_Init`

## 关键组件

| 组件 | ARM | VM |
| --- | --- | --- |
| 显示 | ST7789 通过软件 SPI | SDL2 纹理，2 倍放大 |
| 输入 | ADC 摇杆 + GPIO 按键 | WASD/方向键 + 空格 |
| 音频 | PWM 蜂鸣器 | 空操作 / 控制台输出 |
| Flash | W25Q32 通过硬件 SPI | RAM 模拟 |
| 时基 | SysTick | SDL_GetTicks |
| RTOS | FreeRTOS 内核 | POSIX 线程 + 条件变量 |

## 操作方式

```
WASD/方向键 → 摇杆
空格 → 按键（短按=确认，长按=返回）
ESC → 退出
```

## 局限

- 不模拟 SPI 时序 / ADC 噪声 / Flash 磨损
- 不提供硬实时保证（best-effort pthread）
- 硬件相关 bug 仍需真机测试

## 构建与运行

```bash
# 1. 在 config/config.yaml 配置，platform: VM
# 2. 构建并运行
python3 scripts/cc.py
./build/vm/framework_vm
```

## ADR

详见 [../en/adr/architecture_decisions.md §5](../en/adr/architecture_decisions.md#5-sdl2-vm-simulator)。
