# 09 — Porting & Module Development

## Porting Guide

分层架构使移植系统化：只需修改 BSP，HAL 需微调，APP 零改动。

### What Changes

| Layer | Effort | What |
| --- | --- | --- |
| APP | None | 完全复用 |
| HAL | Low | 更新引脚索引，调整时序常量 |
| BSP | Complete | 用新 MCU SDK 重写 6 个外设模块 |
| DriverLib | Replace | 新 MCU 的 CMSIS + SDK + 链接脚本 |
| Middleware | None | FreeRTOS/LVGL/LFS/RTT 全可移植 |

### Per-Target Effort

| Target | BSP | Config | Total |
| --- | --- | --- | --- |
| MSPM0L (同系列) | 1-2天 | 0.5天 | 1.5-2.5天 |
| STM32F1/F4 | 3-5天 | 1-2天 | 4-7天 |
| GD32 | 3-5天 | 1-2天 | 4-7天 |
| ESP32 | 5-10天 | 2-3天 | 7-13天 |

### Porting Sequence

1. GPIO + Time（最简单）→ 2. PWM → 3. ADC → 4. SPI → 5. UART → 6. 集成测试

### Verification

每个 BSP 模块有对应测试（`src/test/`），按开关逐模块验证。

## Module Development Convention

### Directory Pattern

```
src/<layer>/<module>/
├── <module>.c
├── <module>.h
└── <module>_def.h   (optional, >20 private defines)
```

### Naming

| Element | Convention | Example |
| --- | --- | --- |
| Public function | `Module_Verb()` | `Joystick_Create()` |
| Init | `Module_Init()` | `Bsp_Init()` |
| Update | `Module_Update_All()` | `Button_Update_All()` |
| Static function | `snake_case()` | `read_direction()` |
| Macro | `UPPER_SNAKE_CASE` | `PWM_BUZZER_IDX` |

### HAL Module Template

```c
// my_module.h
typedef struct { uint32_t param; } My_module_config;
typedef struct My_module_t My_module;
My_module* My_Create(const My_module_config* config);
void My_Update_All(void);
```

### Game Template

```c
void My_Game_Init(const Game_hardware* hw);
Game_result My_Game_Update(const Game_input* input);
uint32_t My_Game_Get_Score(void);
uint8_t My_Game_Is_Finished(void);
```

Registration: enum + descriptor in `game_registry.c` + icon in `game_console.c`. See [10_developer_guide.md](10_developer_guide.md) for full walkthrough.

### Rules

- **APP/HAL 禁止 include DriverLib**
- **BSP 函数保持 ≤30 行**（薄封装）
- **新模块必须有 VM stub**
- **功能门控用 `#if MACRO`**（不用 `#ifdef`，宏始终定义为 0/1）
- **配置来自 config 文件**，不硬编码
