# FreeRTOS 设计

项目使用 FreeRTOS 组织运行时任务。ARM 目标使用 `lib/freertos` 中的真实内核与 `portable/GCC/ARM_CM0` 移植层；VM 目标使用 `src/vm/freertos/freertos_stubs.c` 中的 pthread/SDL stub 模拟常用任务 API。

## 初始化与启动

主函数先完成平台、BSP、HAL、APP 初始化，然后定义任务，最后启动平台：

```text
Hal_Task_Def()
Test_Task_Def()
App_Task_Def()
Platform_Start()
```

ARM 上的 `Platform_Start()` 调用 `vTaskStartScheduler()`；VM 上的 `Platform_Start()` 调用 `Vm_Freertos_Start_Tasks()` 并进入 SDL 主循环。

## 现有任务

| 任务 | 来源 | 周期 | 作用 |
| --- | --- | --- | --- |
| `Gpio_Task` | `src/hal/hal.c` | 10 ms | 更新简单 LED、呼吸 LED、按键状态 |
| `Feedback_Task` | `src/hal/hal.c` | 5 ms | 更新蜂鸣器和振动马达播放状态 |
| `Game` | `src/app/game_console/game_console.c` | 5 ms | 游戏控制台主循环、菜单、输入、游戏更新、FPS、屏保 |
| 测试任务 | `src/test` | 由测试模块决定 | 单独验证外设或模块功能 |
| Flash 管理任务 | `src/app/flash_mgr` | 由 `FLASH_MGR_ENABLE` 控制 | 外部 Flash 相关管理逻辑 |

## HAL 周期任务

`Hal_Task_Def()` 创建两个基础任务：

```c
xTaskCreate(task_gpio, "Gpio_Task", 128, NULL, 1, &task_gpio_handle);
xTaskCreate(task_feedback, "Feedback_Task", 128, NULL, 1, &task_feedback_handle);
```

设计意图：

- 输入扫描和反馈播放从游戏逻辑中拆出，避免每个游戏重复处理按键消抖、LED、蜂鸣器和振动。
- 反馈任务周期更短，使短音效和短振动更及时。
- 游戏任务只读取抽象后的输入状态，并调用统一反馈接口。

## 游戏任务

`Game_Console_Task_Def()` 创建 `Game` 任务，栈大小为 1024，优先级为 1。该任务负责：

- 初始化 LCD、Joystick、A/B/X/Y/START 按键、蜂鸣器和振动马达对象。
- 显示 3x2 菜单网格和分页。
- 维护菜单、游戏信息页、游戏运行、暂停、结束菜单等状态。
- 统一处理 X/B 暂停、A 确认、B 返回等交互。
- 调用当前游戏的 `init/update/get_score/is_finished` 接口。
- 维护 FPS 显示和 30 秒无操作屏保。

## VM 中的 FreeRTOS stub

VM 不运行真实 FreeRTOS 内核，而是模拟项目使用到的 API：

- `xTaskCreate()`：记录任务函数。
- `Vm_Freertos_Start_Tasks()`：用 pthread 启动已记录任务。
- `xTaskGetTickCount()`：基于 `SDL_GetTicks()` 返回 tick。
- `vTaskDelay()` / `vTaskDelayUntil()`：基于 `SDL_Delay()`。
- `pvPortMalloc()` / `vPortFree()`：映射到 `malloc()` / `free()`。

该设计的目标不是完全模拟 FreeRTOS，而是让应用逻辑在桌面环境下可运行、可调试。

## 开发注意事项

1. 任务周期应明确，优先使用 `vTaskDelayUntil()` 保持稳定节拍。
2. 短周期任务不要做大量阻塞 I/O 或整屏刷新。
3. 新任务应放在对应模块的 `*_Task_Def()` 中统一创建。
4. VM stub 未完整实现队列等所有 FreeRTOS 功能，新增复杂 RTOS 特性时需要同时补齐 VM 支持。
5. 栈大小需要结合 `.map`、运行日志和 `uxTaskGetStackHighWaterMark()` 分析。
