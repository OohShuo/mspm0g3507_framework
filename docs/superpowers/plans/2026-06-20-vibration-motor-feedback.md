# Vibration Motor Feedback Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a safe, reusable, non-blocking vibration-motor HAL and conservative tactile feedback to the game-console events specified in `agent_prompt_vib_motor.md`.

**Architecture:** A standalone `Vib_Motor` HAL owns PWM output, effect patterns, priorities, cooldown, and instance state. The existing 5 ms buzzer task updates it, while one motor pointer is injected through `Game_hardware`; hardware code is replaced by a complete no-op VM implementation on desktop builds.

**Tech Stack:** C99, FreeRTOS, project `Vector`/allocator utilities, PWM BSP, CMake/Ninja, SDL2 VM, host GCC test harness.

## Global Constraints

- GPIO/PWM never drives the motor directly; hardware uses a transistor or MOSFET with flyback or suitable protection.
- Default PWM frequency is 20,000 Hz, maximum duty is 60%, master strength is 70%, and minimum retrigger interval is 20 ms.
- All vibration playback is non-blocking and is advanced only by `Vib_Motor_Update_All()`.
- Only `src/hal/vib_motor/vib_motor.c` may use `Bsp_Pwm_*` for the motor; games call `Vib_Motor_*` through `Game_hardware`.
- Low-priority effects do not interrupt higher-priority effects; high-frequency gameplay events are deliberately omitted.
- Existing buzzer behavior remains intact.
- `agent_prompt_vib_motor.md` remains untracked and must not be included in commits.

---

## File Map

- `src/hal/vib_motor/vib_motor.h`: public types, defaults, effects, and API.
- `src/hal/vib_motor/vib_motor.c`: instance registry, pattern engine, PWM scaling, cooldown, and priorities.
- `tests/vib_motor/*`: host fakes and behavioral test executable for the real HAL source.
- `src/hal/hal.c`: initialize and update the motor in the shared feedback task.
- `src/vm/hal/vib_motor_vm.c`, `src/vm/CMakeLists.txt`: complete VM substitution.
- `src/test/vib_motor/*`, `config/test_config.h`, `src/test/test.c`: manual on-device feedback sequence.
- `src/app/game_console/game_runtime.h`, `game_console.c`, `game_over_menu.[ch]`: object injection and shell navigation feedback.
- `src/app/games/**`: one-shot feedback at the prompt's named event transitions.

### Task 1: Public HAL Contract and Host Test Harness

**Files:**
- Create: `src/hal/vib_motor/vib_motor.h`
- Create: `tests/vib_motor/test_vib_motor.c`
- Create: `tests/vib_motor/fakes/FreeRTOS.h`
- Create: `tests/vib_motor/fakes/task.h`
- Create: `tests/vib_motor/fakes/bsp_pwm.h`
- Create: `tests/vib_motor/fakes/bsp_time.h`
- Create: `tests/vib_motor/fakes/freertos_alloc.h`
- Create: `tests/vib_motor/fakes/vector.h`
- Create: `tests/vib_motor/fakes/fakes.c`
- Create: `tests/vib_motor/run_tests.sh`

**Interfaces:**
- Produces: the `Vib_Motor_effect`, `Vib_Motor_config`, opaque `Vib_Motor`, four default macros, and all `Vib_Motor_*` functions specified in the design.
- Test controls: `Fake_Time_Set(uint32_t)`, `Fake_Time_Advance(uint32_t)`, and recorded PWM start/stop/frequency/duty values indexed by PWM channel.

- [ ] **Step 1: Add the public contract**

Define all fourteen effects from `vib_effect_menu_tick` through `vib_effect_defeat`, plus `vib_effect_count`. Publish:

```c
#define VIB_MOTOR_DEFAULT_PWM_FREQ_HZ       20000u
#define VIB_MOTOR_DEFAULT_MAX_DUTY_PERCENT  60u
#define VIB_MOTOR_DEFAULT_MASTER_STRENGTH   70u
#define VIB_MOTOR_MIN_RETRIGGER_MS          20u

typedef struct {
    uint8_t pwm_idx;
    uint32_t pwm_freq_hz;
    uint8_t max_duty_percent;
    uint8_t master_strength_percent;
} Vib_Motor_config;
```

Use the exact API listed in the task prompt, with `Vib_Motor_effect` as the effect parameter type.

- [ ] **Step 2: Write the first failing host test**

The fake layer supplies controllable tick time, a simple fixed-capacity vector, `pvPortMalloc`, no-op critical sections, and PWM call recording. The initial test must call `Vib_Motor_Init()`, create PWM index 1, request 100% for 25 ms, and assert frequency 20,000 Hz and duty `0.42f` (70% master × 60% max).

- [ ] **Step 3: Run it and confirm RED**

Run:

```bash
bash tests/vib_motor/run_tests.sh
```

Expected: link failure for missing `Vib_Motor_*` implementation, proving the test reaches the new contract.

- [ ] **Step 4: Commit the contract and harness**

```bash
git add src/hal/vib_motor/vib_motor.h tests/vib_motor
git commit -m "test: define vibration motor HAL behavior"
```

### Task 2: Non-Blocking HAL Core

**Files:**
- Create: `src/hal/vib_motor/vib_motor.c`
- Modify: `tests/vib_motor/test_vib_motor.c`

**Interfaces:**
- Consumes: PWM BSP, millisecond tick, FreeRTOS allocator/critical sections, and `Vector`.
- Produces: all hardware implementations declared in `vib_motor.h`.

- [ ] **Step 1: Implement only creation and direct-pulse scaling**

Create a global `Vector *vib_motor_instances`; initialize it for `Vib_Motor *`; allocate and zero each instance; clamp config percentages; fall back to 20 kHz for zero frequency. Implement `output_strength()` using integer scaling before `Bsp_Pwm_Set_Duty((float)duty / 100.0f)`.

- [ ] **Step 2: Run the first test and confirm GREEN**

Run `bash tests/vib_motor/run_tests.sh`.

Expected: the scaling test passes.

- [ ] **Step 3: Add failing duration/stop/disable tests**

Tests must verify output remains active at 24 ms, stops at 25 ms after `Vib_Motor_Update_All()`, `Vib_Motor_Stop()` stops immediately, disabling stops immediately, disabled playback never starts, and null API calls do not crash.

- [ ] **Step 4: Implement direct-pulse state transitions**

Store active/output state, step start tick, duration, priority, and last accepted-play tick. Use unsigned `now - started_at`. `Set_Master_Strength()` clamps and reapplies the current pulse; setting it to zero silences output. `Set_Enabled(0)` calls stop under a critical section.

- [ ] **Step 5: Run tests and confirm GREEN**

Run `bash tests/vib_motor/run_tests.sh`.

Expected: scaling, timed shutdown, stop, enable, and null-safety tests pass.

- [ ] **Step 6: Add failing cooldown and priority tests**

Assert a second request inside 20 ms is rejected, a request at 20 ms is accepted, lower priority cannot replace an active higher priority, and higher priority can replace an active lower priority after cooldown.

- [ ] **Step 7: Implement the effect library and policies**

Use static const step arrays with the prompt values. Life-lost is `45%/80 ms`, `40 ms` gap, `35%/80 ms`; victory is two `30%/40 ms` pulses separated by `50 ms`. Add the exact 1–5 priority table and validate `effect < vib_effect_count`.

- [ ] **Step 8: Add failing multi-step tests, then implement advancement**

For victory, assert PWM on at 0–39 ms, off at 40–89 ms, on at 90–129 ms, and stopped at 130 ms. Implement recursive or looped advancement so a delayed update can cross multiple completed phases without leaving stale output active.

- [ ] **Step 9: Run all HAL tests and commit**

```bash
bash tests/vib_motor/run_tests.sh
git add src/hal/vib_motor tests/vib_motor
git commit -m "feat: add non-blocking vibration motor HAL"
```

### Task 3: HAL Scheduler, VM Stub, and Manual Device Test

**Files:**
- Modify: `src/hal/hal.c`
- Create: `src/vm/hal/vib_motor_vm.c`
- Modify: `src/vm/CMakeLists.txt`
- Create: `src/test/vib_motor/test_vib_motor.h`
- Create: `src/test/vib_motor/test_vib_motor.c`
- Modify: `config/test_config.h`
- Modify: `src/test/test.c`

**Interfaces:**
- Consumes: Task 2 HAL API.
- Produces: automatic 5 ms updates, VM link parity, and `Test_Vib_Motor_Task_Def()`.

- [ ] **Step 1: Verify the VM implementation is absent**

Run:

```bash
test -f src/vm/hal/vib_motor_vm.c
```

Expected: nonzero exit because the required platform substitute does not exist yet. Then add `src/hal/vib_motor` to the VM public include paths so the new source can include the real public contract.

- [ ] **Step 2: Implement the complete VM stub**

Use a static dummy opaque pointer, no-op play/update/stop calls, and stored master/enabled values so getters remain deterministic. Do not print on every effect.

- [ ] **Step 3: Share the 5 ms feedback task**

In `hal.c`, include `vib_motor.h`, call `Vib_Motor_Init()` in `Hal_Init()`, rename task variables/functions/label from buzzer to feedback, and call both update functions before the existing `vTaskDelayUntil(...5 ms)`.

- [ ] **Step 4: Add the manual test task**

Create the motor on `PWM_VIB_MOTOR_IDX` using public defaults. Print and play menu tick, select, heavy hit, and victory with delays of 1000, 1500, 2500, and 3000 ms. Assert creation succeeds.

- [ ] **Step 5: Wire the disabled-by-default test flag**

Add `TEST_VIB_MOTOR_ENABLE 0`, include it in `TEST_ANY_ENABLE`, include its header in `src/test/test.c`, and dispatch `Test_Vib_Motor_Task_Def()` under `#if TEST_VIB_MOTOR_ENABLE`.

- [ ] **Step 6: Build both platforms and commit**

```bash
python3 scripts/cc.py --target vm
python3 scripts/cc.py --target arm
git add src/hal/hal.c src/vm src/test/vib_motor config/test_config.h src/test/test.c
git commit -m "feat: schedule and stub vibration feedback"
```

Expected: both targets compile and link; no VM vibration log spam.

### Task 4: Console and Game-Over Injection

**Files:**
- Modify: `src/app/game_console/game_runtime.h`
- Modify: `src/app/game_console/game_console.c`
- Modify: `src/app/game_console/game_over_menu.h`
- Modify: `src/app/game_console/game_over_menu.c`

**Interfaces:**
- Consumes: `Vib_Motor *` and preset effects.
- Produces: `Game_hardware.vib_motor` and `Game_Over_Menu_Open(..., Vib_Motor *vib_motor, ...)`.

- [ ] **Step 1: Add a compile-time failing injection change**

Add `#include "vib_motor.h"` and `Vib_Motor *vib_motor` to `Game_hardware`, then build VM before adding its initializer.

Run `python3 scripts/cc.py --target vm`.

Expected: compiler diagnostics identify missing or mismatched integration sites.

- [ ] **Step 2: Create and inject the console motor**

Create `g_vib_motor` using `PWM_VIB_MOTOR_IDX` and public defaults in `console_init()`, assert it, and include it in the `Game_hardware` initializer.

- [ ] **Step 3: Mirror console shell feedback**

Add menu tick beside menu-move buzzer, menu select before entering an item, back when leaving pause/game-over paths, and action light on screensaver wake. Do not vibrate continuously while the screensaver runs.

- [ ] **Step 4: Extend game-over menu ownership**

Store `Vib_Motor *g_vib_motor`; add tick beside each successful cursor move, select beside each accepted confirmation, and back before each return/cancel transition. Extend the open signature and its sole caller.

- [ ] **Step 5: Build VM and commit**

```bash
python3 scripts/cc.py --target vm
git add src/app/game_console
git commit -m "feat: inject vibration feedback into game console"
```

### Task 5: Core Arcade Game Feedback

**Files:**
- Modify: `src/app/games/pacman/pacman.c`
- Modify: `src/app/games/snake/snake.c`
- Modify: `src/app/games/tank_battle/tank_battle.c`
- Modify: `src/app/games/air_battle/air_battle.c`
- Modify: `src/app/games/tetris/tetris.c`
- Modify: `src/app/games/breakout/breakout.c`
- Modify: `src/app/games/pong/pong.c`

**Interfaces:**
- Consumes: `g_hardware.vib_motor` and effect enum.
- Produces: one-shot tactile feedback at existing state transitions.

- [ ] **Step 1: Add Pac-Man and Snake event calls**

Pac-Man: pickup on power pellet, hit-light on ghost eaten, life-lost on lost life, victory/defeat on final states; do not touch ordinary pellet/waka paths. Snake: pickup on food and victory/defeat only.

- [ ] **Step 2: Add Tank and Air Battle event calls**

Tank: shot on fire, hit-light on a successful hit, hit-heavy/life-lost on explosion or lost life. Air: pickup, hit-heavy in `hit_player()`, and victory/defeat in `finish_game()`; do not add vibration to the ordinary continuous-fire path.

- [ ] **Step 3: Add Tetris, Breakout, and Pong event calls**

Tetris: action-light on successful rotate, hit-light on settle/lock, merge on line clear, defeat on game over. Breakout: hit-light only on brick collision, victory on last brick, defeat on last life; no wall vibration. Pong: score on either score and victory/defeat at match end; no paddle/wall vibration.

- [ ] **Step 4: Build and inspect high-frequency exclusions**

```bash
python3 scripts/cc.py --target vm
rg -n "Vib_Motor_Play" src/app/games/{pacman,snake,tank_battle,air_battle,tetris,breakout,pong}
```

Expected: no calls in ordinary pellet, continuous-fire, wall-bounce, or paddle-bounce branches.

- [ ] **Step 5: Commit**

```bash
git add src/app/games/{pacman,snake,tank_battle,air_battle,tetris,breakout,pong}
git commit -m "feat: add tactile feedback to arcade games"
```

### Task 6: Puzzle, Runner, and Dodge Feedback

**Files:**
- Modify: `src/app/games/gomoku/gomoku.c`
- Modify: `src/app/games/game_2048/game_2048.c`
- Modify: `src/app/games/dino_runner/dino_runner.c`
- Modify: `src/app/games/flappy_bird/flappy_bird.c`
- Modify: `src/app/games/maze/maze.c`
- Modify: `src/app/games/needle/needle.c`
- Modify: `src/app/games/dodge_box/dodge_box.c`

**Interfaces:**
- Consumes: `g_hardware.vib_motor`.
- Produces: the remaining game-specific acceptance feedback.

- [ ] **Step 1: Add Gomoku and 2048 calls**

Gomoku uses action-light only after a valid placement and victory on win. 2048 records whether a move merged tiles, plays merge once after the completed move, and plays defeat at the terminal transition; plain moves do not vibrate.

- [ ] **Step 2: Add Dino and Flappy calls**

Dino plays jump on an accepted jump and life-lost/defeat exactly once on collision. Flappy plays jump on accepted flap, score on passing a pipe, and life-lost/defeat once on collision.

- [ ] **Step 3: Add Maze and Needle calls**

Maze optionally uses menu-tick only after a successful grid movement and victory on goal. Needle uses shot on launch, hit-light on a successful stick, and defeat on collision.

- [ ] **Step 4: Add Dodge Box acceptance calls**

Play menu-select on start/restart, hit-heavy or defeat when hit, and victory when a level is cleared. Never call vibration from warning-line rendering or every update frame.

- [ ] **Step 5: Build VM and commit**

```bash
python3 scripts/cc.py --target vm
git add src/app/games/{gomoku,game_2048,dino_runner,flappy_bird,maze,needle,dodge_box}
git commit -m "feat: add tactile feedback to puzzle and runner games"
```

### Task 7: Utility Menu Feedback

**Files:**
- Modify: `src/app/games/calculator/calculator.c`
- Modify: `src/app/games/info/info.c`
- Modify: `src/app/games/sfx_lib/sfx_lib.c`
- Modify: `src/app/games/volume_control/volume_control.c`

**Interfaces:**
- Consumes: stored `Game_hardware` or an added `Vib_Motor *` member.
- Produces: menu tick/select/back behavior without changing utility semantics.

- [ ] **Step 1: Add tick/select/back beside accepted utility inputs**

Use tick when a cursor/value selection actually changes, select when an action is accepted, and back immediately before returning `game_result_exit`. For Volume, vibration must remain available even when buzzer volume is zero.

- [ ] **Step 2: Build and commit**

```bash
python3 scripts/cc.py --target vm
git add src/app/games/{calculator,info,sfx_lib,volume_control}
git commit -m "feat: add tactile feedback to console utilities"
```

### Task 8: Full Verification and Knowledge-Graph Refresh

**Files:**
- Modify as required by diagnostics only.
- Refresh: `graphify-out/*` through the project-required update command.

- [ ] **Step 1: Run the host HAL behavior suite**

Run `bash tests/vib_motor/run_tests.sh`.

Expected: all scaling, timing, gap, priority, cooldown, stop, disable, and null tests pass.

- [ ] **Step 2: Build both production targets from current config**

```bash
python3 scripts/cc.py --target vm
python3 scripts/cc.py --target arm
```

Expected: `build/vm/framework_vm` and `build/arm/framework.elf` link successfully without new warnings attributable to this feature.

- [ ] **Step 3: Compile the enabled manual-test configuration**

Temporarily change only `TEST_VIB_MOTOR_ENABLE` to 1, build ARM, then restore it to 0 with `apply_patch` and rebuild ARM. Do not commit the enabled value.

- [ ] **Step 4: Enforce architectural and safety scans**

```bash
rg -n "Bsp_Pwm_" src/app src/test/vib_motor
rg -n "vTaskDelay" src/hal/vib_motor src/app/games src/app/game_console
rg -n "Vib_Motor_Play" src/app/games src/app/game_console
git diff --check
```

Expected: no app-layer PWM motor control, no blocking delay inside the vibration HAL/game feedback paths, and calls only at intended one-shot events.

- [ ] **Step 5: Refresh graphify and review the final diff**

```bash
graphify update .
git status --short
git diff --stat HEAD~7
```

Confirm `agent_prompt_vib_motor.md` remains untracked and excluded.

- [ ] **Step 6: Request code review and make the final verification commit if needed**

Use the `requesting-code-review` skill, address verified findings, rerun Steps 1–4, and commit only necessary fixes:

```bash
git add src tests config graphify-out
git commit -m "test: verify vibration motor feedback"
```
