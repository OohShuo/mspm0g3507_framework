# Vibration Motor Feedback Design

## Goal

Add a reusable, conservative, non-blocking vibration-motor HAL and use it for lightweight tactile feedback in the game console events named in `agent_prompt_vib_motor.md`. The GPIO/PWM signal is assumed to drive a transistor or MOSFET stage with suitable flyback or other protection; it must not drive the motor directly.

## Scope

The implementation covers:

- a hardware `Vib_Motor` HAL with instance management, timed patterns, priorities, cooldown, enable control, and master strength;
- a shared 5 ms HAL feedback task for buzzer and motor updates;
- propagation through `Game_hardware` and the game-over menu;
- event feedback for the console, game-over screens, and the games/utilities explicitly listed in the task prompt;
- a manual hardware test task and a VM stub;
- automated behavioral checks where the existing build/test structure permits them;
- hardware and VM build verification.

Rhythm and FPS Test receive no application-specific vibration beyond the outer console navigation because they are not named in the task prompt. The removed Racing game is not restored.

## Public HAL Interface

Create `src/hal/vib_motor/vib_motor.h` and `src/hal/vib_motor/vib_motor.c`. The public API provides:

- `Vib_Motor_Init()` and `Vib_Motor_Create()`;
- `Vib_Motor_Play()` for one pulse with explicit strength and duration;
- `Vib_Motor_Play_Effect()` for the fourteen named preset effects;
- `Vib_Motor_Stop()` and `Vib_Motor_Update_All()`;
- master-strength setter/getter;
- enabled setter/getter.

The header exposes these conservative defaults so console and tests share one definition:

- PWM frequency: 20,000 Hz;
- maximum duty: 60%;
- master strength: 70%;
- minimum retrigger interval: 20 ms.

Configuration values are copied into an opaque instance allocated with the project FreeRTOS allocator and registered in the existing `Vector` instance-list style. Creation rejects a null configuration or use before global initialization. Strength, master strength, and maximum duty are clamped to 0–100; a zero configured frequency falls back to 20,000 Hz.

## Pattern Engine and PWM Safety

Each pattern is a sequence of `{strength_percent, duration_ms, gap_ms}` steps. A step starts its PWM output when first visited, turns output off after its duration, waits its gap with PWM stopped, then advances. The update routine obtains time from `Bsp_Get_Tick_Ms()` and uses unsigned subtraction so tick wraparound remains safe. Neither play API blocks or delays.

Effective duty is calculated as:

`requested strength × master strength × maximum duty / 10000`

Intermediate arithmetic uses a type wide enough to avoid overflow. A requested strength of zero, disabled instance, zero master strength, completion, or explicit stop always calls the internal output stop path. Starting a nonzero pulse configures the instance PWM to 20 kHz (or its configured value), sets the scaled duty, and starts it. Only this HAL may call `Bsp_Pwm_*` for motor control.

The default 60% maximum duty and 70% master strength keep nominal full-effect output at 42% actual PWM duty. This software limit complements, but does not replace, the external transistor/MOSFET and motor protection circuit.

## Effects, Priority, and Retriggering

The effect library follows the exact strengths and timings in the task prompt, including the two-pulse life-lost and victory patterns. Priorities are:

- 1: menu movement;
- 2: normal action, jump, and shot;
- 3: pickup, score, and merge;
- 4: light/heavy hit;
- 5: life lost, victory, and defeat.

An active effect rejects a lower-priority effect. Equal or higher priority may replace it after the 20 ms cooldown. Retriggers inside 20 ms are ignored to prevent noisy event loops from holding the motor on. Direct `Vib_Motor_Play()` is treated as a normal action and obeys the same cooldown and active-effect policy. `Vib_Motor_Stop()` is never rate-limited.

Critical sections protect state transitions shared between the game task and the 5 ms feedback task. PWM calls occur consistently inside the same protected transition model used by the buzzer HAL.

## HAL and Runtime Integration

`Hal_Init()` initializes vibration instances. The buzzer task becomes a shared feedback task that calls both `Buzzer_Update_All()` and `Vib_Motor_Update_All()` every 5 ms, without adding another RTOS task.

The game console creates one motor on `PWM_VIB_MOTOR_IDX` with the public defaults. `Game_hardware` gains a `Vib_Motor *vib_motor` member, and every game continues to receive hardware through its existing init callback. `Game_Over_Menu_Open()` additionally accepts and stores the motor pointer.

Null motor pointers remain safe at the HAL boundary, so game and VM paths degrade to no tactile feedback rather than crashing.

## Event Mapping

The console uses menu tick for selection movement, menu select for entering an item, back for returning/cancelling, and optionally action light when waking from the screensaver. The game-over prompt, keyboard, and leaderboard mirror their existing buzzer navigation calls with tick/select/back vibration.

Game integrations are placed near existing buzzer/event transitions and avoid large logic rewrites:

- Pac-Man: power pellet, ghost hit, life lost, victory, defeat; ordinary pellets do not vibrate.
- Snake: food pickup, victory, defeat.
- Tank Battle: shot, hit, explosion/life lost.
- Air Battle: pickup, player hit/explosion, victory, defeat; continuous fire does not vibrate.
- Tetris: optional rotate/action, lock hit, line merge, defeat.
- Breakout: brick hit, victory, defeat; wall bounces do not vibrate.
- Pong: score and final result; paddle/wall bounces do not vibrate.
- Gomoku: valid placement and victory.
- 2048: successful merge and defeat; plain sliding does not vibrate.
- Dino Runner: jump and collision/defeat.
- Flappy Bird: flap, pipe score, collision/defeat.
- Maze: optional successful movement and goal victory.
- Needle: launch, successful stick, collision defeat.
- Dodge Box: start/restart select, hit/defeat, and level victory; warning lines do not vibrate.
- Calculator, Info, SFX, and Volume: menu-style tick/select/back only.

Where a game has no distinct success/failure state beyond the outer game-over transition, feedback is emitted at the nearest existing one-shot transition and never on every frame. Existing buzzer feedback remains unchanged.

## Test and VM Design

Add `src/test/vib_motor/test_vib_motor.h` and `.c`. The disabled-by-default `TEST_VIB_MOTOR_ENABLE` flag joins `TEST_ANY_ENABLE` and dispatches the task from `src/test/test.c`. The task creates the default motor and cycles through menu tick, select, heavy hit, and victory with one-to-three-second pauses and diagnostic prints. It never requests 100% duty or continuous output.

Add `src/vm/hal/vib_motor_vm.c` implementing every public API as safe no-ops with stable default getter behavior. Add the vibration HAL include directory to the VM target. VM playback remains silent to avoid log spam.

Automated HAL behavior checks use controllable time and PWM observations where practical. They cover:

- strength/master/max-duty scaling and clamping;
- non-blocking turnoff after duration;
- gaps and multi-step advancement;
- lower-priority rejection and higher-priority interruption;
- the 20 ms cooldown;
- immediate stop/disable and safe null calls.

## Verification

Acceptance requires:

1. hardware build succeeds;
2. VM build succeeds;
3. the manual vibration test compiles both disabled and enabled;
4. gameplay code contains no direct motor `Bsp_Pwm_*` calls;
5. menu, Dino, Flappy, and Dodge Box feedback matches the prompt;
6. all vibration timing is managed by `Vib_Motor_Update_All()` without blocking delays;
7. the knowledge graph is refreshed with `graphify update .` after code changes.

Hardware feel cannot be proven in the desktop environment. The manual test and conservative defaults provide the on-device verification path; final strength tuning remains a configuration adjustment rather than a game-code change.
