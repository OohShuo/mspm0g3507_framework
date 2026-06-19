# 10 — Developer Guide

## API Index

### BSP

| Module | Header | Key Functions |
| --- | --- | --- |
| GPIO | `bsp_gpio.h` | `Init`, `Write(idx,state)`, `Read(idx)`, `Toggle(idx)` |
| PWM | `bsp_pwm.h` | `Init`, `Set_Duty(idx,float)`, `Set_Freq(idx,hz)`, `Start/Stop(idx)` |
| ADC | `bsp_adc.h` | `Init`, `Read_Voltage(idx,ch)→float`, `Start/Stop`, DMA cb |
| Hard SPI | `bsp_hard_spi.h` | `Init`, `Write/Read(idx,data,len)`, `Write/Read_Blocking`, DMA cbs |
| Soft SPI | `bsp_soft_spi.h` | `Init`, `Write(idx,data,len)` |
| UART | `bsp_uart.h` | `Init`, `Write/Read`, `Start_Continuous_Rx`, idle cb |
| Time | `bsp_time.h` | `Get_Tick_Ms()→uint32_t` |

### HAL

| Module | Key Functions | Config Struct |
| --- | --- | --- |
| ST7789 | `Create`, `Init`, `Flush`, `Begin_Write`, `Write_Pixels`, `End_Write`, `Set_Backlight` | `spi_idx`, `dc/rst/bkl_gpio_idx`, `hor/ver_res`, flags |
| W25Q32 | `Create`, `Init`, `Read`, `Page_Program`, `Sector_Erase`, `Block_Erase_32K/64K`, `Chip_Erase` | `spi_idx`, `cs_gpio_idx` |
| Joystick | `Create`, `Calibrate_Center` | `adc_idx`, voltage range, dead zone, reverse |
| Button | `Create`, `Get_State→up/down` | `gpio_idx`, `gpio_state_when_pressed` |
| Buzzer | `Create`, `Play_Sfx(idx)`, `Play(music)`, `Stop`, `Set_Volume` | `pwm_idx` |
| LED Simple | `Init`, `On/Off/Toggle`, `Start_Blink/Stop_Blink` | — |
| LED Breath | `Init`, `Start/Stop` | — |
| COM UART | `Create`, `Send` | `uart_idx`, protocol config |

### APP

| Module | Key Functions |
| --- | --- |
| Storage | `Init`, `Raw_Read/Write/Erase`, `Get_Lfs`, `Format`, `Lock/Unlock` |
| Game Console | `Game_descriptor`, `Game_input`, `Game_hardware`, `Game_result` |
| Game Graphics | `Fill_Rect`, `Draw_Text`, `Draw_U32`, `Draw_Bitmap`, `Draw_Gray4_Bitmap` |
| Score Store | `Init`, `Get`, `Add`, `Commit`, `Get_Entry` |

## Common Tasks

### Add a Game

1. `src/app/games/my_game/my_game.{c,h}` — implement `Init/Update/Get_Score/Is_Finished`
2. `game_registry.h` — add `game_icon_my_game` to `Game_icon`, `game_id_my_game` to `Game_id`
3. `game_registry.c` — add `Game_descriptor` entry
4. `game_console.c` — add icon draw function + case in `draw_grid_cell()`
5. Test on VM: `python3 scripts/cc.py --target vm && ./build/vm/framework_vm` (`--target` matches the `name:` field in config.yaml)

### Add a HAL Module

1. `src/hal/my_module/my_module.{c,h}` — config struct + `Create/Init/Update`
2. Register in `Hal_Init()` or provide standalone `_Create()`
3. Add to `src/hal/CMakeLists.txt`
4. VM stub: `src/vm/hal/my_module_vm.c`

### Add a BSP Module

1. `src/bsp/my_periph/bsp_my_periph.{c,h}` — indexed wrappers around DriverLib
2. Define constants in `board_config.h`
3. Register in `Bsp_Init()`
4. Add to `src/bsp/CMakeLists.txt`
5. VM stub: `src/vm/bsp/bsp_my_periph.c`

### Add a Test

1. `src/test/my_feature/test_my_feature.{c,h}`
2. Add `#define TEST_MY_FEATURE_ENABLE 0` in `test_config.h`
3. Add `#if` block in `test.c`
4. Add to `src/test/CMakeLists.txt`

## Testing

14 independent test modules, controlled via `test_config.h` switches:

```
TEST_BUTTON / TEST_BUZZER / TEST_COM_UART / TEST_JOYSTICK / TEST_LCD
TEST_LED_BREATH / TEST_LED_SIMPLE / TEST_LFS / TEST_LVGL_BALL
TEST_LVGL_HELLO / TEST_RTT / TEST_SLIP_RECV / TEST_ST7789_IMG / TEST_W25Q32
```

All disabled by default. `Test_Task_Def()` creates test tasks (priority 1).

VM is the primary test platform: `printf` output is directly visible, LCD renders in a PC window, keyboard input simulates joystick/button.

## Naming & Return Convention

- Public: `PascalCase_Verb_Noun()` — `Joystick_Create()`
- BSP: `Bsp_Periph_Action()` — `Bsp_Gpio_Write()`
- Static: `snake_case()`
- Return: `uint8_t` (0=fail, 1=ok), pointer (NULL=fail), `void` (cannot fail or fatal via `configASSERT`)
