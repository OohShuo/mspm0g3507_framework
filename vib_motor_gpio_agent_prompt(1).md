# Agent 提示词：将振动马达从 PWM 驱动切换为 GPIO 驱动

## 任务背景

当前工程中已经存在一个基于 PWM 的振动马达 HAL：

- `src/hal/vib_motor/vib_motor.h`
- `src/hal/vib_motor/vib_motor.c`
- `src/vm/hal/vib_motor_vm.c`
- `src/test/vib_motor/test_vib_motor.c`

当前 APP 层主要通过下面的接口触发振动：

```c
Vib_Motor_Play_Effect(g_hardware.vib_motor, vib_effect_xxx);
Vib_Motor_Play(g_hardware.vib_motor, strength, duration_ms);
Vib_Motor_Update_All();
```

当前硬件配置中，振动马达使用 PWM：

```c
#define PWM_VIB_MOTOR_IDX 1
```

现在需要把振动马达设计调整为两套驱动：

1. `vib_motor_pwm`：保留当前 PWM 版本，用于以后可能需要强度控制的场景。
2. `vib_motor_gpio`：新增 GPIO 版本，用 GPIO 输出高低电平驱动振动马达，实现更简单的开/关式震动效果。

最终要求：**APP 层的振动马达调用切换为 GPIO 驱动版本。**

---

## 总体目标

完成以下改造：

1. 将当前 `vib_motor` PWM 版重命名/迁移为 `vib_motor_pwm`。
2. 新增 `vib_motor_gpio` 驱动模块。
3. GPIO 版本不需要 PWM 强度控制，只需要按时长和次数播放简单震动模式。
4. 保留类似原有 `vib_effect_xxx` 的效果枚举，让 APP 层调用逻辑尽量少改。
5. 将 APP 层的振动马达对象和调用切换到 `vib_motor_gpio`。
6. 更新 `board_config.h`，新增振动马达 GPIO 索引，例如 `GPIO_VIB_MOTOR_IDX`。
7. 更新 VM 版本，使桌面模拟仍然能看到/感知振动事件。
8. 更新测试代码，增加 GPIO 振动马达测试。

---

## 重要硬件约束

虽然软件层叫 `vib_motor_gpio`，但是不要在注释或文档里鼓励 GPIO 直接带电机负载。

应明确说明：

- MCU GPIO 只输出控制信号。
- 实际硬件上应通过三极管或 MOSFET 驱动振动马达。
- 振动马达两端或驱动级需要合适的续流/保护设计。
- `GPIO_VIB_MOTOR_IDX` 对应的是驱动管控制脚，不是电机直接供电脚。

---

## 一、目录与文件改造

### 1. 保留当前 PWM 版本为 `vib_motor_pwm`

将当前 PWM 版本从：

```text
src/hal/vib_motor/
```

迁移或复制为：

```text
src/hal/vib_motor_pwm/
```

建议文件名：

```text
src/hal/vib_motor_pwm/vib_motor_pwm.h
src/hal/vib_motor_pwm/vib_motor_pwm.c
```

原有 PWM 版中的类型和函数名也同步改名，避免和 GPIO 版冲突。

示例：

```c
Vib_motor              -> Vib_motor_pwm
Vib_motor_config       -> Vib_motor_pwm_config
Vib_motor_effect       -> Vib_motor_pwm_effect
Vib_motor_step         -> Vib_motor_pwm_step
Vib_motor_pattern      -> Vib_motor_pwm_pattern

Vib_Motor_Init         -> Vib_Motor_Pwm_Init
Vib_Motor_Create       -> Vib_Motor_Pwm_Create
Vib_Motor_Play         -> Vib_Motor_Pwm_Play
Vib_Motor_Play_Effect  -> Vib_Motor_Pwm_Play_Effect
Vib_Motor_Stop         -> Vib_Motor_Pwm_Stop
Vib_Motor_Update_All   -> Vib_Motor_Pwm_Update_All
```

PWM 版本仍然使用：

```c
#include "bsp_pwm.h"
```

并继续通过：

```c
Bsp_Pwm_Set_Freq(...)
Bsp_Pwm_Set_Duty(...)
Bsp_Pwm_Start(...)
Bsp_Pwm_Stop(...)
```

控制振动马达。

PWM 版本可以暂时不接入 APP，只要保证能编译通过即可。

---

### 2. 新增 GPIO 版本 `vib_motor_gpio`

新增目录：

```text
src/hal/vib_motor_gpio/
```

新增文件：

```text
src/hal/vib_motor_gpio/vib_motor_gpio.h
src/hal/vib_motor_gpio/vib_motor_gpio.c
```

GPIO 版本使用：

```c
#include "bsp_gpio.h"
#include "bsp_time.h"
```

不再使用：

```c
#include "bsp_pwm.h"
```

---

## 二、GPIO 版接口设计

GPIO 版应提供一组和原 `vib_motor` 类似的接口，方便 APP 层迁移。

建议头文件 `vib_motor_gpio.h`：

```c
#pragma once

#include <stdint.h>

#define VIB_MOTOR_GPIO_MIN_RETRIGGER_MS 20u

typedef enum {
    vib_gpio_effect_menu_tick = 0,
    vib_gpio_effect_menu_select,
    vib_gpio_effect_back,
    vib_gpio_effect_action_light,
    vib_gpio_effect_jump,
    vib_gpio_effect_shot,
    vib_gpio_effect_pickup,
    vib_gpio_effect_score,
    vib_gpio_effect_merge,
    vib_gpio_effect_hit_light,
    vib_gpio_effect_hit_heavy,
    vib_gpio_effect_life_lost,
    vib_gpio_effect_victory,
    vib_gpio_effect_defeat,
    vib_gpio_effect_count
} Vib_motor_gpio_effect;

typedef struct {
    uint8_t gpio_idx;
    uint8_t active_high;
    uint8_t enabled;
} Vib_motor_gpio_config;

typedef struct {
    uint16_t on_ms;
    uint16_t off_ms;
} Vib_motor_gpio_step;

typedef struct {
    const Vib_motor_gpio_step* steps;
    uint8_t length;
} Vib_motor_gpio_pattern;

typedef struct Vib_motor_gpio_t {
    Vib_motor_gpio_config config;

    uint8_t active;
    uint8_t output_on;
    uint8_t priority;
    uint8_t has_played;
    uint8_t step_index;
    uint8_t in_gap;

    uint32_t started_at;
    uint32_t last_play_at;

    uint16_t duration_ms;
    const Vib_motor_gpio_pattern* pattern;
} Vib_motor_gpio;

void Vib_Motor_Gpio_Init(void);
Vib_motor_gpio* Vib_Motor_Gpio_Create(const Vib_motor_gpio_config* config);

void Vib_Motor_Gpio_Play(Vib_motor_gpio* obj, uint16_t duration_ms);
void Vib_Motor_Gpio_Play_Effect(Vib_motor_gpio* obj, Vib_motor_gpio_effect effect);
void Vib_Motor_Gpio_Stop(Vib_motor_gpio* obj);
void Vib_Motor_Gpio_Update_All(void);

void Vib_Motor_Gpio_Set_Enabled(Vib_motor_gpio* obj, uint8_t enabled);
uint8_t Vib_Motor_Gpio_Is_Enabled(Vib_motor_gpio* obj);
```

说明：

- GPIO 版不需要 `strength_percent`。
- GPIO 版只需要开/关。
- 原来 `Vib_Motor_Play(obj, strength, duration)` 可以迁移为 `Vib_Motor_Gpio_Play(obj, duration)`。
- 原来 `Vib_Motor_Play_Effect(obj, effect)` 可以迁移为 `Vib_Motor_Gpio_Play_Effect(obj, gpio_effect)`。

---

## 三、GPIO 版振动模式设计

GPIO 版本按“短震 / 长震 / 多段组合”实现反馈。

建议模式表：

```c
/* 单次短震 */
menu_tick:      12 ms
menu_select:   35 ms
back:          45 ms
action_light:  18 ms
jump:          18 ms
shot:          20 ms
pickup:        30 ms
score:         25 ms
merge:         35 ms
hit_light:     45 ms
hit_heavy:     90 ms

/* 生命损失：短 + 短 */
life_lost:     80 ms on, 40 ms off, 80 ms on

/* 胜利：短 + 短，节奏轻快 */
victory:       40 ms on, 50 ms off, 40 ms on

/* 失败：短 + 短 + 长 */
defeat:        60 ms on, 40 ms off, 60 ms on, 40 ms off, 160 ms on
```

失败效果重点实现“短短长”。

可以这样定义：

```c
#define SINGLE_GPIO_STEP(name, on_time) \
    static const Vib_motor_gpio_step name##_steps[] = {{on_time, 0u}}; \
    static const Vib_motor_gpio_pattern name = {name##_steps, 1u}

SINGLE_GPIO_STEP(pattern_menu_tick, 12u);
SINGLE_GPIO_STEP(pattern_menu_select, 35u);
SINGLE_GPIO_STEP(pattern_back, 45u);
SINGLE_GPIO_STEP(pattern_action_light, 18u);
SINGLE_GPIO_STEP(pattern_jump, 18u);
SINGLE_GPIO_STEP(pattern_shot, 20u);
SINGLE_GPIO_STEP(pattern_pickup, 30u);
SINGLE_GPIO_STEP(pattern_score, 25u);
SINGLE_GPIO_STEP(pattern_merge, 35u);
SINGLE_GPIO_STEP(pattern_hit_light, 45u);
SINGLE_GPIO_STEP(pattern_hit_heavy, 90u);

static const Vib_motor_gpio_step pattern_life_lost_steps[] = {
    {80u, 40u},
    {80u, 0u},
};

static const Vib_motor_gpio_step pattern_victory_steps[] = {
    {40u, 50u},
    {40u, 0u},
};

static const Vib_motor_gpio_step pattern_defeat_steps[] = {
    {60u, 40u},
    {60u, 40u},
    {160u, 0u},
};
```

---

## 四、GPIO 输出逻辑

GPIO 输出函数应封装 active high / active low。

```c
static void output_on(Vib_motor_gpio* obj) {
    if (obj == NULL || !obj->config.enabled) {
        return;
    }

    Bsp_Gpio_Write(
        obj->config.gpio_idx,
        obj->config.active_high ? bsp_gpio_state_set : bsp_gpio_state_reset
    );

    obj->output_on = 1u;
}

static void output_off(Vib_motor_gpio* obj) {
    if (obj == NULL) {
        return;
    }

    Bsp_Gpio_Write(
        obj->config.gpio_idx,
        obj->config.active_high ? bsp_gpio_state_reset : bsp_gpio_state_set
    );

    obj->output_on = 0u;
}
```

注意：

- 初始化后默认关闭输出。
- 停止振动时必须关闭 GPIO。
- 禁用振动时必须关闭 GPIO。
- `Vib_Motor_Gpio_Update_All()` 应放在反馈任务里周期调用。
- 当前工程 `Hal_Task_Def()` 里已经有 `task_feedback`，周期 5 ms，可继续使用。

---

## 五、优先级与防抖逻辑

GPIO 版保留原 PWM 版类似的优先级逻辑，避免低优先级震动打断高优先级震动。

建议优先级：

```c
static const uint8_t gpio_effect_priorities[vib_gpio_effect_count] = {
    1u, /* menu_tick */
    2u, /* menu_select */
    2u, /* back */
    2u, /* action_light */
    2u, /* jump */
    2u, /* shot */
    3u, /* pickup */
    3u, /* score */
    3u, /* merge */
    4u, /* hit_light */
    4u, /* hit_heavy */
    5u, /* life_lost */
    5u, /* victory */
    5u, /* defeat */
};
```

防止过于频繁触发：

```c
#define VIB_MOTOR_GPIO_MIN_RETRIGGER_MS 20u
```

规则：

1. 如果当前有高优先级效果在播放，低优先级效果不能打断。
2. 如果在最小重触发间隔内，低优先级或同优先级效果不重复触发。
3. 高优先级效果允许打断低优先级效果。
4. `Stop` 和 `Set_Enabled(false)` 必须立即关闭输出。

---

## 六、`board_config.h` 修改要求

当前 GPIO 索引已经使用到 0~9：

```c
#define GPIO_NUM 10
```

需要新增一个 GPIO 输出给振动马达控制脚。

示例：

```c
#define GPIO_NUM 11

#define GPIO_10_PORT GPIOB
#define GPIO_10_PIN  DL_GPIO_PIN_xx
#define GPIO_10_MODE GPIO_MODE_OUTPUT
```

同时更新数组宏：

```c
#define GPIO_PORTS { ... , GPIO_10_PORT }
#define GPIO_PINS  { ... , GPIO_10_PIN  }
#define GPIO_MODES { ... , GPIO_10_MODE }
```

新增索引名：

```c
#define GPIO_VIB_MOTOR_IDX 10
```

注意：

- `DL_GPIO_PIN_xx` 需要根据实际 PCB 引脚修改。
- 不要复用已经用于按键、LCD、SPI CS、电源 LED 的 GPIO。
- 如果实际硬件暂时未确定引脚，可以先写 TODO 注释，但代码结构必须完整。

保留 PWM 配置：

```c
#define PWM_VIB_MOTOR_IDX 1
```

但建议改名为：

```c
#define PWM_VIB_MOTOR_PWM_IDX 1
```

如果改名会引起大量影响，也可以暂时保留 `PWM_VIB_MOTOR_IDX`，但 PWM 版本内部应改为 `vib_motor_pwm` 命名，避免和 GPIO 版混淆。

---

## 七、HAL 初始化与任务修改

当前 `src/hal/hal.c` 中：

```c
#include "vib_motor.h"
...
Vib_Motor_Init();
...
Vib_Motor_Update_All();
```

需要改为 GPIO 版：

```c
#include "vib_motor_gpio.h"
...
Vib_Motor_Gpio_Init();
...
Vib_Motor_Gpio_Update_All();
```

如果 PWM 版暂时不启用，就不要在 `Hal_Init()` 和 `task_feedback()` 中调用 PWM 版更新函数。

目标是：

```c
void Hal_Init(void) {
    Led_Simple_Init();
    Led_Breath_Init();
    Button_Init();
    Joystick_Init();
    Buzzer_Init();
    Vib_Motor_Gpio_Init();
#if FRAMEWORK_USE_UART
    Com_Uart_Init();
#endif
}
```

```c
static void task_feedback(void* arg) {
    uint32_t tick = xTaskGetTickCount();

    while (1) {
        Buzzer_Update_All();
        Vib_Motor_Gpio_Update_All();

        vTaskDelayUntil(&tick, pdMS_TO_TICKS(5));
    }
}
```

---

## 八、APP 层调用迁移

当前 APP 层的硬件结构在：

```text
src/app/game_console/game_runtime.h
```

原来是：

```c
#include "vib_motor.h"

typedef struct {
    St7789* lcd;
    Buzzer* buzzer;
    Vib_motor* vib_motor;
} Game_hardware;
```

改为：

```c
#include "vib_motor_gpio.h"

typedef struct {
    St7789* lcd;
    Buzzer* buzzer;
    Vib_motor_gpio* vib_motor;
} Game_hardware;
```

---

### 1. 创建对象处修改

当前 `src/app/game_console/game_console.c` 中大致有：

```c
const Vib_motor_config vib_motor_config = {
    .pwm_idx = PWM_VIB_MOTOR_IDX,
    .pwm_freq_hz = VIB_MOTOR_DEFAULT_PWM_FREQ_HZ,
    .max_duty_percent = VIB_MOTOR_DEFAULT_MAX_DUTY_PERCENT,
    .master_strength_percent = VIB_MOTOR_DEFAULT_MASTER_STRENGTH,
};

g_vib_motor = Vib_Motor_Create(&vib_motor_config);
```

改为：

```c
const Vib_motor_gpio_config vib_motor_config = {
    .gpio_idx = GPIO_VIB_MOTOR_IDX,
    .active_high = 1u,
    .enabled = 1u,
};

g_vib_motor = Vib_Motor_Gpio_Create(&vib_motor_config);
```

---

### 2. APP 调用替换规则

批量替换：

```c
Vib_Motor_Play_Effect(...)
```

为：

```c
Vib_Motor_Gpio_Play_Effect(...)
```

批量替换：

```c
Vib_Motor_Play(obj, strength, duration_ms)
```

为：

```c
Vib_Motor_Gpio_Play(obj, duration_ms)
```

批量替换：

```c
Vib_Motor_Stop(...)
```

为：

```c
Vib_Motor_Gpio_Stop(...)
```

批量替换：

```c
Vib_Motor_Set_Enabled(...)
Vib_Motor_Is_Enabled(...)
```

为：

```c
Vib_Motor_Gpio_Set_Enabled(...)
Vib_Motor_Gpio_Is_Enabled(...)
```

---

### 3. 效果枚举迁移

原来：

```c
vib_effect_menu_tick
vib_effect_menu_select
vib_effect_back
vib_effect_action_light
vib_effect_jump
vib_effect_shot
vib_effect_pickup
vib_effect_score
vib_effect_merge
vib_effect_hit_light
vib_effect_hit_heavy
vib_effect_life_lost
vib_effect_victory
vib_effect_defeat
```

改为：

```c
vib_gpio_effect_menu_tick
vib_gpio_effect_menu_select
vib_gpio_effect_back
vib_gpio_effect_action_light
vib_gpio_effect_jump
vib_gpio_effect_shot
vib_gpio_effect_pickup
vib_gpio_effect_score
vib_gpio_effect_merge
vib_gpio_effect_hit_light
vib_gpio_effect_hit_heavy
vib_gpio_effect_life_lost
vib_gpio_effect_victory
vib_gpio_effect_defeat
```

也可以为了减少 APP 改动，在 `vib_motor_gpio.h` 里提供兼容宏：

```c
#define vib_effect_menu_tick      vib_gpio_effect_menu_tick
#define vib_effect_menu_select    vib_gpio_effect_menu_select
#define vib_effect_back           vib_gpio_effect_back
#define vib_effect_action_light   vib_gpio_effect_action_light
#define vib_effect_jump           vib_gpio_effect_jump
#define vib_effect_shot           vib_gpio_effect_shot
#define vib_effect_pickup         vib_gpio_effect_pickup
#define vib_effect_score          vib_gpio_effect_score
#define vib_effect_merge          vib_gpio_effect_merge
#define vib_effect_hit_light      vib_gpio_effect_hit_light
#define vib_effect_hit_heavy      vib_gpio_effect_hit_heavy
#define vib_effect_life_lost      vib_gpio_effect_life_lost
#define vib_effect_victory        vib_gpio_effect_victory
#define vib_effect_defeat         vib_gpio_effect_defeat
```

如果使用兼容宏，APP 层只需要替换函数名和类型即可，游戏代码里的 `vib_effect_xxx` 可以暂时不改。

推荐做法：

- HAL 内部使用新名字。
- 对 APP 提供兼容宏，减少一次性修改风险。
- 后续再逐步清理旧名字。

---

## 九、VM 适配

当前 VM 有：

```text
src/vm/hal/vib_motor_vm.c
```

建议新增或改名为：

```text
src/vm/hal/vib_motor_gpio_vm.c
```

VM 版本不需要真实 GPIO，只需要保留模拟振动反馈：

```c
Vm_Haptics_Set_Strength(0u);
Vm_Haptics_Set_Strength(100u);
```

GPIO 版 VM 中：

- `output_on()` 时调用 `Vm_Haptics_Set_Strength(100u)`。
- `output_off()` 时调用 `Vm_Haptics_Set_Strength(0u)`。
- 仍然使用同样的 pattern、priority、update 逻辑。

如果暂时不想大改 VM，可以让 VM 的 `vib_motor_gpio` 实现复用原 `vib_motor_vm.c` 的时间调度逻辑，只是把强度统一映射为 0 或 100。

---

## 十、测试程序修改

当前已有：

```text
src/test/vib_motor/test_vib_motor.c
src/test/vib_motor/test_vib_motor.h
```

建议新增：

```text
src/test/vib_motor_gpio/test_vib_motor_gpio.c
src/test/vib_motor_gpio/test_vib_motor_gpio.h
```

测试内容：

1. 初始化 GPIO 振动马达。
2. 播放单次短震。
3. 播放 `menu_select`。
4. 播放 `victory`。
5. 播放 `defeat`，确认效果为“短短长”。
6. 测试 `Set_Enabled(false)` 后不会输出。
7. 测试播放中 `Stop()` 能立即关闭 GPIO。

示例测试流程：

```c
void Test_Vib_Motor_Gpio(void) {
    Vib_motor_gpio_config config = {
        .gpio_idx = GPIO_VIB_MOTOR_IDX,
        .active_high = 1u,
        .enabled = 1u,
    };

    Vib_motor_gpio* motor = Vib_Motor_Gpio_Create(&config);

    Vib_Motor_Gpio_Play(motor, 50u);
    vTaskDelay(pdMS_TO_TICKS(500));

    Vib_Motor_Gpio_Play_Effect(motor, vib_gpio_effect_menu_select);
    vTaskDelay(pdMS_TO_TICKS(500));

    Vib_Motor_Gpio_Play_Effect(motor, vib_gpio_effect_victory);
    vTaskDelay(pdMS_TO_TICKS(1000));

    Vib_Motor_Gpio_Play_Effect(motor, vib_gpio_effect_defeat);
    vTaskDelay(pdMS_TO_TICKS(1500));

    Vib_Motor_Gpio_Set_Enabled(motor, 0u);
    Vib_Motor_Gpio_Play_Effect(motor, vib_gpio_effect_hit_heavy);
    vTaskDelay(pdMS_TO_TICKS(500));

    Vib_Motor_Gpio_Set_Enabled(motor, 1u);
    Vib_Motor_Gpio_Play(motor, 1000u);
    vTaskDelay(pdMS_TO_TICKS(100));
    Vib_Motor_Gpio_Stop(motor);
}
```

---

## 十一、CMake 修改

需要检查当前 CMake 是否自动收集源码。

如果是手动列文件，需要把新增文件加入构建：

```text
src/hal/vib_motor_gpio/vib_motor_gpio.c
src/hal/vib_motor_pwm/vib_motor_pwm.c
src/vm/hal/vib_motor_gpio_vm.c
src/test/vib_motor_gpio/test_vib_motor_gpio.c
```

并移除或不再直接编译旧路径：

```text
src/hal/vib_motor/vib_motor.c
src/vm/hal/vib_motor_vm.c
```

如果项目通过 `file(GLOB_RECURSE ...)` 自动收集，也要确保旧文件不会和新文件重复定义同名函数。

重点检查：

- ARM target 能编译。
- VM target 能编译。
- TEST target 能编译。
- 不出现 `Vib_Motor_*` 和 `Vib_Motor_Gpio_*` 重名冲突。
- 不出现旧 `vib_motor.h` 被 APP 继续包含的问题。

---

## 十二、推荐兼容策略

为了减少一次改动导致的编译错误，可以采用两阶段策略。

### 阶段 1：结构迁移

1. 把旧 PWM 版迁移为 `vib_motor_pwm`。
2. 新增 `vib_motor_gpio`。
3. `Game_hardware` 里的振动对象改为 `Vib_motor_gpio*`。
4. APP 创建对象改为 `Vib_Motor_Gpio_Create()`。
5. HAL 初始化和更新改为 GPIO 版。

### 阶段 2：APP 调用清理

1. 将 APP 中所有 `Vib_Motor_Play_Effect` 改为 `Vib_Motor_Gpio_Play_Effect`。
2. 将 APP 中所有 `Vib_Motor_Play` 改为 `Vib_Motor_Gpio_Play`。
3. 将 APP 中所有 `Vib_Motor_Stop` 改为 `Vib_Motor_Gpio_Stop`。
4. 暂时使用兼容宏保留 `vib_effect_xxx`。
5. 编译通过后，再考虑是否把 `vib_effect_xxx` 全部改成 `vib_gpio_effect_xxx`。

---

## 十三、需要重点检查的 APP 文件

当前工程中调用振动马达的文件可能包括：

```text
src/app/game_console/game_console.c
src/app/game_console/game_runtime.h
src/app/game_console/game_over_menu.c
src/app/game_console/game_over_menu.h

src/app/games/info/info.c
src/app/games/needle/needle.c
src/app/games/air_battle/air_battle.c
src/app/games/dodge_box/dodge_box.c
src/app/games/breakout/breakout.c
src/app/games/dino_runner/dino_runner.c
src/app/games/flappy_bird/flappy_bird.c
src/app/games/sfx_lib/sfx_lib.c
src/app/games/volume_control/volume_control.c
src/app/games/calculator/calculator.c
src/app/games/game_2048/game_2048.c
src/app/games/gomoku/gomoku.c
src/app/games/pacman/pacman.c
src/app/games/pong/pong.c
src/app/games/snake/snake.c
src/app/games/tank_battle/tank_battle.c
src/app/games/tetris/tetris.c
src/app/games/maze/maze.c
```

请用全局搜索确认：

```bash
grep -R "Vib_Motor" -n src config
grep -R "vib_effect" -n src config
grep -R "vib_motor.h" -n src config
```

迁移完成后，应尽量只剩下：

```text
Vib_Motor_Gpio_*
vib_motor_gpio.h
vib_gpio_effect_*
```

如果为了兼容暂时保留 `vib_effect_xxx` 宏，也要保证旧的 `vib_motor.h` 不再被 APP 层使用。

---

## 十四、验收标准

完成后必须满足：

1. `vib_motor_pwm` 和 `vib_motor_gpio` 是两个独立模块。
2. PWM 版本仍保留原有强度控制能力。
3. GPIO 版本只通过 GPIO 高低电平开关控制振动。
4. GPIO 版支持简单模式播放：
   - 短震
   - 长震
   - 多段震动
   - 失败“短短长”
   - 胜利“双短震”
5. APP 层使用 GPIO 版振动马达对象。
6. APP 层游戏逻辑不再依赖 PWM 强度。
7. `Hal_Init()` 使用 `Vib_Motor_Gpio_Init()`。
8. `task_feedback()` 使用 `Vib_Motor_Gpio_Update_All()`。
9. `board_config.h` 增加 `GPIO_VIB_MOTOR_IDX`。
10. VM 目标可编译，并能模拟 GPIO 振动。
11. ARM 目标可编译。
12. TEST 目标可编译。
13. 播放结束、禁用、停止时 GPIO 必须回到关闭状态。
14. 不允许出现 GPIO 输出一直保持开启导致马达持续震动的问题。

---

## 十五、实现注意事项

- 不要在中断里延时等待振动结束。
- 不要使用阻塞式 `delay` 实现模式播放。
- 使用当前工程已有的 `Bsp_Get_Tick_Ms()` 或 FreeRTOS tick 做非阻塞更新。
- `Vib_Motor_Gpio_Update_All()` 由反馈任务周期调用。
- 多段模式通过状态机实现，不要一次性阻塞执行。
- 如果使用动态内存，沿用工程当前的 `pvPortMalloc` 和 `Vector` 风格。
- 如果只需要一个 GPIO 振动马达，也可以像 VM 一样使用静态对象，但要保持接口清晰。
- 风格上尽量和当前 `vib_motor.c` 保持一致，减少不必要的大规模重构。

---

## 十六、建议提交说明

提交信息可以写：

```text
feat(hal): add gpio vibration motor driver

- rename pwm vibration motor driver to vib_motor_pwm
- add gpio-based vibration motor driver
- switch app haptics calls to gpio vibration driver
- add simple on/off vibration patterns
- add defeat short-short-long feedback pattern
- update board config with GPIO_VIB_MOTOR_IDX
- update vm haptics simulation for gpio vibration
```
