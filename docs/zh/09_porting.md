# 09 — 移植与模块开发

## 移植指南

分层架构使移植系统化：只需修改 BSP，HAL 需微调，APP 零改动。

### 各层变更

| 层 | 工作量 | 内容 |
| --- | --- | --- |
| APP | 无 | 完全复用 |
| HAL | 低 | 更新引脚索引，调整时序常量 |
| BSP | 全部 | 用新 MCU SDK 重写 6 个外设模块 |
| DriverLib | 替换 | 新 MCU 的 CMSIS + SDK + 链接脚本 |
| 中间件 | 无 | FreeRTOS/LVGL/LFS/RTT 全可移植 |

### 各目标工作量

| 目标 | BSP | 配置 | 总计 |
| --- | --- | --- | --- |
| MSPM0L（同系列） | 1-2天 | 0.5天 | 1.5-2.5天 |
| STM32F1/F4 | 3-5天 | 1-2天 | 4-7天 |
| GD32 | 3-5天 | 1-2天 | 4-7天 |
| ESP32 | 5-10天 | 2-3天 | 7-13天 |

### 移植顺序

1. GPIO + 时基（最简单）→ 2. PWM → 3. ADC → 4. SPI → 5. UART → 6. 集成测试

### 验证

每个 BSP 模块有对应测试（`src/test/`），按开关逐模块验证。

## 模块开发规范

### 目录模式

```
src/<层级>/<模块>/
├── <模块>.c
├── <模块>.h
└── <模块>_def.h   （可选，超过 20 个私有定义时使用）
```

### 命名

| 元素 | 约定 | 示例 |
| --- | --- | --- |
| 公开函数 | `模块_动词()` | `Joystick_Create()` |
| 初始化 | `模块_Init()` | `Bsp_Init()` |
| 更新 | `模块_Update_All()` | `Button_Update_All()` |
| 静态函数 | `snake_case()` | `read_direction()` |
| 宏 | `UPPER_SNAKE_CASE` | `PWM_BUZZER_IDX` |

### HAL 模块模板

```c
// my_module.h
typedef struct { uint32_t param; } My_module_config;
typedef struct My_module_t My_module;
My_module* My_Create(const My_module_config* config);
void My_Update_All(void);
```

### 游戏模板

```c
void My_Game_Init(const Game_hardware* hw);
Game_result My_Game_Update(const Game_input* input);
uint32_t My_Game_Get_Score(void);
uint8_t My_Game_Is_Finished(void);
```

注册：枚举 + `game_registry.c` 中的描述符 + `game_console.c` 中的图标。完整步骤见 [10_developer_guide.md](10_developer_guide.md)。

### 规则

- **APP/HAL 禁止 include DriverLib**
- **BSP 函数保持 ≤30 行**（薄封装）
- **新模块必须有 VM 桩**
- **功能门控用 `#if MACRO`**（不用 `#ifdef`，宏始终定义为 0/1）
- **配置来自 config 文件**，不硬编码
