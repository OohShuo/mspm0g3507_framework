# 09 — Porting & Module Development

## Porting Guide

The layered architecture makes porting systematic: only BSP needs rewriting, HAL needs minor adjustments, APP requires zero changes.

### What Changes

| Layer | Effort | What |
| --- | --- | --- |
| APP | None | Fully reusable |
| HAL | Low | Update pin indices, adjust timing constants |
| BSP | Complete | Rewrite 6 peripheral modules with new MCU SDK |
| DriverLib | Replace | New MCU CMSIS + SDK + linker script |
| Middleware | None | FreeRTOS/LVGL/LFS/RTT are fully portable |

### Porting Sequence

1. GPIO + Time (simplest) → 2. PWM → 3. ADC → 4. SPI → 5. UART → 6. Integration test

### Verification

Each BSP module has a corresponding test (`src/test/`), verified module by module via feature switches.

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

- **APP/HAL must never include DriverLib**
- **BSP functions must stay ≤30 lines** (thin wrappers)
- **New modules must have a VM stub**
- **Feature gate with `#if MACRO`** (not `#ifdef`, macros are always defined as 0/1)
- **Configuration comes from config files**, not hardcoded
