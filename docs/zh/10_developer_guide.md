# 10 — 开发者指南

## API 索引

### BSP

| 模块 | 头文件 | 关键函数 |
| --- | --- | --- |
| GPIO | `bsp_gpio.h` | `Init`、`Write(idx,state)`、`Read(idx)`、`Toggle(idx)` |
| PWM | `bsp_pwm.h` | `Init`、`Set_Duty(idx,float)`、`Set_Freq(idx,hz)`、`Start/Stop(idx)` |
| ADC | `bsp_adc.h` | `Init`、`Read_Voltage(idx,ch)→float`、`Start/Stop`、DMA 回调 |
| 硬件 SPI | `bsp_hard_spi.h` | `Init`、`Write/Read(idx,data,len)`、`Write/Read_Blocking`、DMA 回调 |
| 软件 SPI | `bsp_soft_spi.h` | `Init`、`Write(idx,data,len)` |
| UART | `bsp_uart.h` | `Init`、`Write/Read`、`Start_Continuous_Rx`、空闲回调 |
| 时基 | `bsp_time.h` | `Get_Tick_Ms()→uint32_t` |

### HAL

| 模块 | 关键函数 | 配置结构体 |
| --- | --- | --- |
| ST7789 | `Create`、`Init`、`Flush`、`Begin_Write`、`Write_Pixels`、`End_Write`、`Set_Backlight` | `spi_idx`、`dc/rst/bkl_gpio_idx`、`hor/ver_res`、flags |
| W25Q32 | `Create`、`Init`、`Read`、`Page_Program`、`Sector_Erase`、`Block_Erase_32K/64K`、`Chip_Erase` | `spi_idx`、`cs_gpio_idx` |
| 摇杆 | `Create`、`Calibrate_Center` | `adc_idx`、电压范围、死区、反向 |
| 按键 | `Create`、`Get_State→up/down` | `gpio_idx`、`gpio_state_when_pressed` |
| 蜂鸣器 | `Create`、`Play_Sfx(idx)`、`Play(music)`、`Stop`、`Set_Volume` | `pwm_idx` |
| LED Simple | `Init`、`On/Off/Toggle`、`Start_Blink/Stop_Blink` | — |
| LED Breath | `Init`、`Start/Stop` | — |
| COM UART | `Create`、`Send` | `uart_idx`、协议配置 |

### APP

| 模块 | 关键函数 |
| --- | --- |
| 存储 | `Init`、`Raw_Read/Write/Erase`、`Get_Lfs`、`Format`、`Lock/Unlock` |
| 游戏控制台 | `Game_descriptor`、`Game_input`、`Game_hardware`、`Game_result` |
| 游戏图形 | `Fill_Rect`、`Draw_Text`、`Draw_U32`、`Draw_Bitmap`、`Draw_Gray4_Bitmap` |
| 分数存储 | `Init`、`Get`、`Add`、`Commit`、`Get_Entry` |

## 常见任务

### 添加游戏

1. `src/app/games/my_game/my_game.{c,h}` — 实现 `Init/Update/Get_Score/Is_Finished`
2. `game_registry.h` — 添加 `game_icon_my_game` 到 `Game_icon`，`game_id_my_game` 到 `Game_id`
3. `game_registry.c` — 添加 `Game_descriptor` 条目
4. `game_console.c` — 添加图标绘制函数 + 在 `draw_grid_cell()` 中添加分支
5. 在 VM 上测试：`python3 scripts/cc.py --target vm && ./build/vm/framework_vm`（`--target` 匹配 config.yaml 中的 `name:`）

### 添加 HAL 模块

1. `src/hal/my_module/my_module.{c,h}` — 配置结构体 + `Create/Init/Update`
2. 在 `Hal_Init()` 中注册或提供独立的 `_Create()`
3. 添加到 `src/hal/CMakeLists.txt`
4. VM 桩：`src/vm/hal/my_module_vm.c`

### 添加 BSP 模块

1. `src/bsp/my_periph/bsp_my_periph.{c,h}` — DriverLib 索引封装
2. 在 `board_config.h` 中定义常量
3. 在 `Bsp_Init()` 中注册
4. 添加到 `src/bsp/CMakeLists.txt`
5. VM 桩：`src/vm/bsp/bsp_my_periph.c`

### 添加测试

1. `src/test/my_feature/test_my_feature.{c,h}`
2. 在 `test_config.h` 中添加 `#define TEST_MY_FEATURE_ENABLE 0`
3. 在 `test.c` 中添加 `#if` 块
4. 添加到 `src/test/CMakeLists.txt`

## 测试

通过 `test_config.h` 开关控制的独立测试模块：

```
TEST_BUTTON / TEST_BUZZER / TEST_COM_UART / TEST_JOYSTICK / TEST_LCD
TEST_LED_BREATH / TEST_LED_SIMPLE / TEST_LFS / TEST_LVGL_BALL
TEST_LVGL_HELLO / TEST_RTT / TEST_SLIP_RECV / TEST_ST7789_IMG / TEST_W25Q32
```

所有默认禁用。`Test_Task_Def()` 创建测试任务（优先级 1）。

VM 是主要测试平台：`printf` 输出直接可见，LCD 渲染在 PC 窗口，键盘输入模拟摇杆/按键。

## 命名与返回值约定

- 公开：`PascalCase_Verb_Noun()` — `Joystick_Create()`
- BSP：`Bsp_Periph_Action()` — `Bsp_Gpio_Write()`
- 静态：`snake_case()`
- 返回：`uint8_t`（0=失败，1=成功），指针（NULL=失败），`void`（不会失败或通过 `configASSERT` 致命错误）
