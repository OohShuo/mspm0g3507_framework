# Agent 任务提示词：加入振动马达 HAL、测试程序和游戏反馈

你要在 `mspm0g3507_framework` 工程中完成振动马达驱动。马达 PWM 端口已经在 `config/board_config.h` 中定义：

```c
#define PWM_VIB_MOTOR_IDX 1
```

请实现一个非阻塞、强度适中、可复用的 HAL 抽象，并在合适的游戏事件中加入轻量振动反馈。不要在游戏代码里直接操作 `Bsp_Pwm_*`，游戏层只能调用 HAL 或通过 `Game_hardware` 使用马达对象。

## 一、当前工程关键信息

已有 PWM 和蜂鸣器设计可参考：

- PWM BSP：
  - `src/bsp/pwm/bsp_pwm.h`
  - `src/bsp/pwm/bsp_pwm.c`
  - 已有接口：`Bsp_Pwm_Set_Duty()`、`Bsp_Pwm_Set_Freq()`、`Bsp_Pwm_Start()`、`Bsp_Pwm_Stop()`。
- 蜂鸣器 HAL：
  - `src/hal/buzzer/buzzer.h`
  - `src/hal/buzzer/buzzer.c`
  - `src/hal/buzzer/buzzer_def.c`
  - 可参考它的“实例列表 + 非阻塞 update + SFX 枚举/优先级”结构。
- HAL 初始化任务：
  - `src/hal/hal.c`
  - 当前 `task_buzzer()` 每 5 ms 调用 `Buzzer_Update_All()`。
- 测试框架：
  - `config/test_config.h`
  - `src/test/test.c`
  - 现有测试例子：`src/test/buzzer/test_buzzer.c`
- VM stub：
  - `src/vm/bsp/bsp_pwm.c`
  - `src/vm/hal/buzzer_vm.c`

## 二、安全和硬件假设

请按以下假设写代码和注释：

1. MSPM0G3507 的 GPIO/PWM 引脚不能直接驱动振动马达。硬件上应通过三极管或 MOSFET 驱动，并加续流二极管或合适的保护电路。
2. 软件层只负责输出 PWM 占空比，不要假设可以直接承受马达电流。
3. 默认振动强度要保守，避免持续强震影响供电、屏幕刷新或用户体验。
4. 马达 PWM 建议使用较高频率，默认 `20000 Hz`，减少可闻噪声。
5. 默认最大占空比建议限制在 `55% ~ 60%`，不要一上来 100%。如果实际马达太弱，后续再通过配置调高。

## 三、需要新增的 HAL 模块

新增目录和文件：

```text
src/hal/vib_motor/vib_motor.h
src/hal/vib_motor/vib_motor.c
```

由于 `src/hal/CMakeLists.txt` 使用 `GLOB_RECURSE`，新文件通常会自动参与编译；仍需确认 include path 是否覆盖了新目录。

### 1. 推荐 API

`vib_motor.h` 建议定义：

```c
#pragma once

#include <stdint.h>

typedef enum {
    vib_effect_menu_tick = 0,
    vib_effect_menu_select,
    vib_effect_back,
    vib_effect_action_light,
    vib_effect_jump,
    vib_effect_shot,
    vib_effect_pickup,
    vib_effect_score,
    vib_effect_merge,
    vib_effect_hit_light,
    vib_effect_hit_heavy,
    vib_effect_life_lost,
    vib_effect_victory,
    vib_effect_defeat,
    vib_effect_count
} Vib_Motor_effect;

typedef struct {
    uint8_t pwm_idx;
    uint32_t pwm_freq_hz;
    uint8_t max_duty_percent;
    uint8_t master_strength_percent;
} Vib_Motor_config;

typedef struct Vib_Motor_t Vib_Motor;

void Vib_Motor_Init(void);
Vib_Motor* Vib_Motor_Create(const Vib_Motor_config* config);

void Vib_Motor_Play(Vib_Motor* obj, uint8_t strength_percent, uint16_t duration_ms);
void Vib_Motor_Play_Effect(Vib_Motor* obj, Vib_Motor_effect effect);
void Vib_Motor_Stop(Vib_Motor* obj);
void Vib_Motor_Update_All(void);

void Vib_Motor_Set_Master_Strength(Vib_Motor* obj, uint8_t strength_percent);
uint8_t Vib_Motor_Get_Master_Strength(Vib_Motor* obj);
void Vib_Motor_Set_Enabled(Vib_Motor* obj, uint8_t enabled);
uint8_t Vib_Motor_Is_Enabled(Vib_Motor* obj);
```

可以根据工程风格调整命名，但必须提供：

- 初始化
- 创建实例
- 播放一次指定强度/时长的振动
- 播放预设效果
- 停止
- 周期更新
- 设置全局强度或开关

### 2. 非阻塞实现要求

`Vib_Motor_Play()` 不允许 `vTaskDelay()` 阻塞。它只设置当前振动状态，真正关断由 `Vib_Motor_Update_All()` 完成。

建议内部结构：

```c
typedef struct {
    uint8_t strength_percent;
    uint16_t duration_ms;
    uint16_t gap_ms;
} Vib_Motor_step;

typedef struct {
    const Vib_Motor_step* steps;
    uint8_t length;
} Vib_Motor_pattern;

struct Vib_Motor_t {
    Vib_Motor_config config;
    const Vib_Motor_pattern* pattern;
    uint8_t step_index;
    uint32_t step_started_at;
    uint8_t active;
    uint8_t output_on;
    uint8_t enabled;
    uint8_t priority;
    uint32_t last_play_at;
};
```

### 3. PWM 输出规则

实现一个内部函数，例如：

```c
static void output_strength(Vib_Motor* obj, uint8_t strength_percent) {
    if (obj == NULL || !obj->enabled || strength_percent == 0) {
        stop_output(obj);
        return;
    }

    if (strength_percent > 100u) strength_percent = 100u;

    uint8_t effective = (uint8_t)((uint16_t)strength_percent * obj->config.master_strength_percent / 100u);
    uint8_t duty_percent = (uint8_t)((uint16_t)effective * obj->config.max_duty_percent / 100u);

    Bsp_Pwm_Stop(obj->config.pwm_idx);
    Bsp_Pwm_Set_Freq(obj->config.pwm_idx, obj->config.pwm_freq_hz);
    Bsp_Pwm_Set_Duty(obj->config.pwm_idx, (float)duty_percent / 100.0f);
    Bsp_Pwm_Start(obj->config.pwm_idx);
}
```

默认配置建议：

```c
#define VIB_MOTOR_DEFAULT_PWM_FREQ_HZ       20000u
#define VIB_MOTOR_DEFAULT_MAX_DUTY_PERCENT  60u
#define VIB_MOTOR_DEFAULT_MASTER_STRENGTH   70u
#define VIB_MOTOR_MIN_RETRIGGER_MS          20u
```

不要默认 100% 强度。游戏反馈大多数应该在 10%~45% 之间。

### 4. 效果库和优先级

请提供一组预设效果，避免每个游戏手写强度和时间。

建议效果表：

| 效果 | 建议模式 | 用途 |
|---|---:|---|
| `vib_effect_menu_tick` | 10%, 12 ms | 菜单移动、光标移动 |
| `vib_effect_menu_select` | 20%, 35 ms | 确认、进入游戏 |
| `vib_effect_back` | 22%, 45 ms | 返回、取消 |
| `vib_effect_action_light` | 12%, 18 ms | 普通动作 |
| `vib_effect_jump` | 14%, 18 ms | Dino / Flappy 跳跃或拍翅 |
| `vib_effect_shot` | 15%, 20 ms | 射击、发射 |
| `vib_effect_pickup` | 22%, 30 ms | 拾取道具 |
| `vib_effect_score` | 18%, 25 ms | 得分、过管、进球 |
| `vib_effect_merge` | 24%, 35 ms | 2048 合并、消行 |
| `vib_effect_hit_light` | 28%, 45 ms | 命中、碰撞 |
| `vib_effect_hit_heavy` | 45%, 90 ms | 爆炸、撞车 |
| `vib_effect_life_lost` | 45%, 80 ms + gap 40 ms + 35%, 80 ms | 掉命、失败前冲击 |
| `vib_effect_victory` | 30%, 40 ms + gap 50 ms + 30%, 40 ms | 胜利 |
| `vib_effect_defeat` | 50%, 120 ms | 失败 |

优先级建议：

- 菜单移动：1
- 普通动作/跳跃/射击：2
- 得分/拾取/合并：3
- 命中/碰撞：4
- 掉命/胜利/失败：5

如果当前有高优先级效果正在播放，低优先级效果不要打断它。

还要加一个最小重触发间隔，例如 20 ms，避免高频事件让马达一直抖。

## 四、接入 HAL 初始化和任务

修改 `src/hal/hal.c`：

1. include 新头文件：

```c
#include "vib_motor.h"
```

2. `Hal_Init()` 中调用：

```c
Vib_Motor_Init();
```

3. 周期更新：

可以把 `task_buzzer` 改名为 `task_feedback`，同时更新蜂鸣器和马达：

```c
static void task_feedback(void* arg) {
    uint32_t tick = xTaskGetTickCount();
    while (1) {
        Buzzer_Update_All();
        Vib_Motor_Update_All();
        vTaskDelayUntil(&tick, pdMS_TO_TICKS(5));
    }
}
```

也可以新增一个 `task_vib_motor`，但没有必要占用更多任务资源。推荐复用 5 ms feedback task。

## 五、接入 Game_hardware

修改 `src/app/game_console/game_runtime.h`：

```c
#include "vib_motor.h"

typedef struct {
    St7789* lcd;
    Buzzer* buzzer;
    Vib_Motor* vib_motor;
} Game_hardware;
```

修改 `src/app/game_console/game_console.c`：

1. 增加全局对象：

```c
static Vib_Motor* g_vib_motor = NULL;
```

2. 在 `console_init()` 创建：

```c
const Vib_Motor_config vib_config = {
    .pwm_idx = PWM_VIB_MOTOR_IDX,
    .pwm_freq_hz = VIB_MOTOR_DEFAULT_PWM_FREQ_HZ,
    .max_duty_percent = VIB_MOTOR_DEFAULT_MAX_DUTY_PERCENT,
    .master_strength_percent = VIB_MOTOR_DEFAULT_MASTER_STRENGTH,
};
g_vib_motor = Vib_Motor_Create(&vib_config);
configASSERT(g_vib_motor != NULL);
```

如果默认宏放在 `.c` 内部，就不要在 `game_console.c` 直接引用宏；可以提供 `Vib_Motor_Default_Config(PWM_VIB_MOTOR_IDX)` 或在头文件公开默认宏。

3. 构造 `Game_hardware` 时加入：

```c
const Game_hardware hardware = {
    .lcd = g_lcd,
    .buzzer = g_buzzer,
    .vib_motor = g_vib_motor,
};
```

## 六、游戏反馈设计

原则：振动只辅助关键事件，不要每一帧或每个高频小事件都振动。尤其 Pac-Man 吃普通豆、Breakout 每次墙反弹、Air Battle 连续开火这类高频事件不要让马达一直震。

### 1. Game Console / 菜单

文件：`src/app/game_console/game_console.c`

- 菜单移动：`vib_effect_menu_tick`
- 选择进入游戏：`vib_effect_menu_select`
- Back 返回/取消：`vib_effect_back`
- 从屏保唤醒：可选 `vib_effect_action_light`

### 2. Game Over 菜单

文件：`src/app/game_console/game_over_menu.c`

当前只保存了 `Buzzer* g_buzzer`。你可以把 `Game_Over_Menu_Open()` 的参数扩展为接收 `Vib_Motor*`，或把震动反馈放在外层 console 中处理。推荐扩展参数：

```c
void Game_Over_Menu_Open(St7789* lcd, Buzzer* buzzer, Vib_Motor* vib_motor, ...);
```

反馈：

- 光标移动：`vib_effect_menu_tick`
- 确认：`vib_effect_menu_select`
- 返回：`vib_effect_back`

### 3. 各游戏建议接入点

根据已有蜂鸣器调用位置，尽量在同一个事件附近加震动。

| 游戏 | 建议反馈 |
|---|---|
| Pac-Man | 能量豆/吃鬼：`pickup` / `hit_light`；掉命/胜利/失败：`life_lost` / `victory` / `defeat`；普通豆不要震 |
| Snake | 吃食物：`pickup`；转向可不震或极轻；失败/胜利：`defeat` / `victory` |
| Racing | 换道：`action_light`；超车：`score`；撞车：`hit_heavy` 或 `defeat` |
| Tank Battle | 发射：`shot`；命中：`hit_light`；爆炸/掉命：`hit_heavy` / `life_lost` |
| Air Battle | 拾取道具：`pickup`；中弹/爆炸：`hit_heavy`；胜负：`victory` / `defeat`；普通连续开火不要频繁震，若要加必须受 cooldown 限制 |
| Tetris | 旋转：可选 `action_light`；落块锁定：`hit_light`；消行/四消：`merge` 或 `victory` 级别短双击；失败：`defeat` |
| Breakout | 打砖：`hit_light`；通关/失败：`victory` / `defeat`；球碰墙不要震 |
| Pong | 得分：`score`；最终胜负：`victory` / `defeat`；普通挡板反弹可不震 |
| Gomoku | 落子：`action_light`；胜利：`victory`；非法落子不要震或轻 tick |
| 2048 | 有合并：`merge`；普通滑动可不震或 `menu_tick`；失败：`defeat` |
| Dino Runner | 跳跃：`jump`；撞击：`life_lost` / `defeat` |
| Flappy Bird | 拍翅：`jump`；过管得分：`score`；撞击：`life_lost` / `defeat` |
| Maze | 成功移动：可选 `menu_tick`；到达终点：`victory`；失败：`defeat` |
| Needle | 发射：`shot`；插中：`hit_light`；碰撞失败：`defeat` |
| Dodge Box | 开始/重开：`menu_select`；被击中：`hit_heavy` 或 `defeat`；过关：`victory`；不要对每个警告线持续震 |
| Calculator / Info / SFX / Volume | 菜单式 tick/select/back 即可 |

实现时不要为了加震动大幅重写游戏逻辑。优先在已有 `Buzzer_Play_Sfx()` 附近加：

```c
Vib_Motor_Play_Effect(g_hardware.vib_motor, vib_effect_xxx);
```

如果某个游戏当前只保存了 `Buzzer*` 而不是完整 `Game_hardware`，请小心扩展其静态硬件对象，不要破坏初始化流程。

## 七、测试程序

新增：

```text
src/test/vib_motor/test_vib_motor.h
src/test/vib_motor/test_vib_motor.c
```

### 1. 测试头文件

```c
#pragma once

void Test_Vib_Motor_Task_Def(void);
```

### 2. 测试行为

测试任务应：

1. 使用 `PWM_VIB_MOTOR_IDX` 创建 `Vib_Motor`。
2. 每隔一段时间播放不同效果，便于肉眼/手感确认。
3. 不要持续无限 100% 输出。
4. 打印提示，便于串口/RTT 观察。

示例流程：

```c
static void vib_motor_task(void* arg) {
    (void)arg;
    const Vib_Motor_config cfg = {
        .pwm_idx = PWM_VIB_MOTOR_IDX,
        .pwm_freq_hz = 20000u,
        .max_duty_percent = 60u,
        .master_strength_percent = 70u,
    };
    Vib_Motor* motor = Vib_Motor_Create(&cfg);
    configASSERT(motor != NULL);

    while (1) {
        printf("[VIB] menu tick\n");
        Vib_Motor_Play_Effect(motor, vib_effect_menu_tick);
        vTaskDelay(pdMS_TO_TICKS(1000));

        printf("[VIB] select\n");
        Vib_Motor_Play_Effect(motor, vib_effect_menu_select);
        vTaskDelay(pdMS_TO_TICKS(1500));

        printf("[VIB] heavy hit\n");
        Vib_Motor_Play_Effect(motor, vib_effect_hit_heavy);
        vTaskDelay(pdMS_TO_TICKS(2500));

        printf("[VIB] victory\n");
        Vib_Motor_Play_Effect(motor, vib_effect_victory);
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}
```

### 3. 接入测试框架

修改 `config/test_config.h`：

```c
#define TEST_VIB_MOTOR_ENABLE 0
```

修改 `TEST_ANY_ENABLE`：

```c
#define TEST_ANY_ENABLE (... || TEST_VIB_MOTOR_ENABLE)
```

修改 `src/test/test.c`：

```c
#include "vib_motor/test_vib_motor.h"

#if TEST_VIB_MOTOR_ENABLE
    Test_Vib_Motor_Task_Def();
#endif
```

由于 `src/test/CMakeLists.txt` 使用 `GLOB_RECURSE`，新增测试文件通常会自动编译。

## 八、VM stub

为了桌面模拟器不链接失败，需要新增或更新 VM stub：

```text
src/vm/hal/vib_motor_vm.c
```

实现所有 `Vib_Motor_*` API。VM 中可以全部 no-op，也可以在播放效果时 `printf("[VM VIB] effect=%d\n", effect);`，建议默认只轻量打印或不打印，避免刷屏。

同时确认 `src/vm/CMakeLists.txt` include path 包含 `src/hal/vib_motor`。

## 九、验收标准

完成后至少满足：

1. 硬件工程能编译通过。
2. VM 工程能编译通过。
3. `TEST_VIB_MOTOR_ENABLE=1` 时，马达能按测试节奏短振，不会持续强震。
4. `TEST_VIB_MOTOR_ENABLE=0` 时，正常游戏不受影响。
5. 菜单移动、确认、返回有轻微反馈。
6. Dino / Flappy 跳跃有短促轻振。
7. Dodge Box 被击中和通关有明显但不过强的反馈。
8. 失败、胜利、爆炸等关键事件反馈比菜单强，但默认不超过中等强度。
9. 游戏代码没有直接调用 `Bsp_Pwm_*` 控制马达。
10. 马达输出由 `Vib_Motor_Update_All()` 非阻塞管理，不能在游戏 update 中阻塞延时。

## 十、实现注意事项

- 不要把马达驱动写进 `buzzer.c`，马达应该是独立 HAL。
- 不要使用阻塞延时播放振动。
- 不要让高频事件持续重触发马达，必须有优先级或 cooldown。
- 默认 duty 和强度要保守；如果用户觉得弱，可以后续调配置。
- 如果马达接在 MOSFET 上，PWM duty 越高越强；如果实际硬件相反，需要在配置里加反相选项，而不是在游戏里硬编码。
- 保留蜂鸣器原有音效，不要用震动替代声音；两者是并行反馈。
