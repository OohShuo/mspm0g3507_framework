# Unified Game Lifecycle and RNG Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the APP console the sole owner of terminal game flow and replace eleven private random generators with one tested, per-game `Game_rng` API.

**Architecture:** `game_runtime` provides the lifecycle result enum and an explicit-state 32-bit RNG. Games return terminal outcomes directly and retain only in-game presentation; `game_console` routes those outcomes into `game_over_menu`, which plays terminal feedback once and owns leaderboard/replay. Registry finish polling is removed after every game has adopted the result contract.

**Tech Stack:** C99, existing APP/HAL APIs, Python 3 `unittest` for source-contract checks, host `cc` for RNG unit tests, existing CMake VM and ARM builds.

## Global Constraints

- Changes are limited to `src/app`, APP-facing documentation, and host tests; no BSP API changes.
- Preserve gameplay rules, scores, controls, pause behavior, and 2048's non-terminal `state_win` behavior.
- Games return `game_result_won` or `game_result_lost` on the exact frame that enters a terminal state.
- The console emits exactly one generic victory/defeat sound and vibration; games retain only feedback for events after which play continues.
- Each randomized game owns an independent `Game_rng`, seeded with `Game_Runtime_Get_Tick_Ms() ^ distinct_nonzero_constant`.
- `Game_Rng_Range(rng, 0)` returns zero; all other bounded results use rejection sampling.
- Keep every intermediate commit buildable for the VM target.

---

### Task 1: Add the Tested Runtime RNG and Lifecycle Values

**Files:**
- Create: `tests/app/game_runtime_rng_test.c`
- Modify: `src/app/game_console/game_runtime.h`
- Modify: `src/app/game_console/game_runtime.c`

**Interfaces:**
- Produces: `Game_rng`, `Game_Rng_Seed(Game_rng *, uint32_t)`, `Game_Rng_Next(Game_rng *)`, and `Game_Rng_Range(Game_rng *, uint32_t)`.
- Produces: `game_result_won` and `game_result_lost` enum values used by Tasks 4–6.

- [ ] **Step 1: Write the failing host RNG test**

Create `tests/app/game_runtime_rng_test.c` with a BSP tick stub and these exact assertions:

```c
#include <assert.h>
#include <stdint.h>

#include "game_runtime.h"

uint32_t Bsp_Get_Tick_Ms(void) { return 0; }

int main(void) {
    Game_rng a;
    Game_rng b;

    Game_Rng_Seed(&a, 0u);
    assert(a.state == 0x6D2B79F5u);

    Game_Rng_Seed(&a, 1u);
    assert(Game_Rng_Next(&a) == 1015568748u);
    assert(Game_Rng_Next(&a) == 1586005467u);
    assert(Game_Rng_Next(&a) == 2165703038u);

    Game_Rng_Seed(&a, 0x12345678u);
    Game_Rng_Seed(&b, 0x12345678u);
    assert(Game_Rng_Next(&a) == Game_Rng_Next(&b));
    (void)Game_Rng_Next(&a);
    assert(a.state != b.state);

    assert(Game_Rng_Range(&a, 0u) == 0u);
    for (uint32_t bound = 1u; bound < 65u; bound++) {
        for (uint32_t i = 0; i < 1000u; i++) { assert(Game_Rng_Range(&a, bound) < bound); }
    }

    Game_Rng_Seed(&a, 1u);
    assert(Game_Rng_Range(&a, 0x80000001u) == 18219389u);
    return 0;
}
```

- [ ] **Step 2: Compile the test and verify the API is missing**

Run:

```bash
cc -std=c99 -Wall -Wextra -Werror \
  -Isrc/app/game_console -Isrc/hal/buzzer -Isrc/hal/st7789 \
  -Isrc/hal/vib_motor_gpio -Isrc/bsp/time \
  tests/app/game_runtime_rng_test.c src/app/game_console/game_runtime.c \
  -o /tmp/game_runtime_rng_test
```

Expected: compilation fails because `Game_rng` and `Game_Rng_*` are not declared.

- [ ] **Step 3: Add the lifecycle values and RNG declarations**

Extend `game_runtime.h` with:

```c
typedef enum {
    game_result_running,
    game_result_exit,
    game_result_won,
    game_result_lost,
} Game_result;

typedef struct {
    uint32_t state;
} Game_rng;

void Game_Rng_Seed(Game_rng* rng, uint32_t seed);
uint32_t Game_Rng_Next(Game_rng* rng);
uint32_t Game_Rng_Range(Game_rng* rng, uint32_t upper_bound);
```

- [ ] **Step 4: Implement the minimal RNG**

Append to `game_runtime.c`:

```c
#define GAME_RNG_ZERO_SEED 0x6D2B79F5u

void Game_Rng_Seed(Game_rng* rng, uint32_t seed) {
    if (rng == NULL) { return; }
    rng->state = seed == 0u ? GAME_RNG_ZERO_SEED : seed;
}

uint32_t Game_Rng_Next(Game_rng* rng) {
    if (rng == NULL) { return 0u; }
    rng->state = rng->state * 1664525u + 1013904223u;
    return rng->state;
}

uint32_t Game_Rng_Range(Game_rng* rng, uint32_t upper_bound) {
    if (rng == NULL || upper_bound == 0u) { return 0u; }
    const uint32_t threshold = (uint32_t)(0u - upper_bound) % upper_bound;
    uint32_t value;
    do { value = Game_Rng_Next(rng); } while (value < threshold);
    return value % upper_bound;
}
```

Also add `<stddef.h>` for `NULL`.

- [ ] **Step 5: Run the host test and VM build**

Run the compile command from Step 2, then:

```bash
/tmp/game_runtime_rng_test
python3 scripts/cc.py --target vm
```

Expected: the test exits 0 and the VM build completes successfully.

- [ ] **Step 6: Commit**

```bash
git add tests/app/game_runtime_rng_test.c src/app/game_console/game_runtime.h src/app/game_console/game_runtime.c
git commit -m "feat: add shared game runtime rng"
```

### Task 2: Migrate the Existing 32-bit LCG Games

**Files:**
- Modify: `src/app/games/snake/snake.c`
- Modify: `src/app/games/tetris/tetris.c`
- Modify: `src/app/games/pacman/pacman.c`
- Modify: `src/app/games/air_battle/air_battle.c`
- Modify: `src/app/games/game_2048/game_2048.c`
- Modify: `src/app/games/tank_battle/tank_battle.c`
- Modify: `src/app/games/dodge_box/dodge_box.c`

**Interfaces:**
- Consumes: the `Game_rng` API from Task 1.
- Produces: seven games with private RNG objects and no private LCG transition helper.

- [ ] **Step 1: Capture the failing migration scan**

Run:

```bash
rg -n 'static uint32_t (random_next|rng_next)|g_random_state|g_rng_state' \
  src/app/games/{snake,tetris,pacman,air_battle,game_2048,tank_battle,dodge_box}
```

Expected: matches in all seven game sources.

- [ ] **Step 2: Replace private words and transition helpers**

In each source replace the private word/helper pair with `static Game_rng g_rng;`. Seed in its existing `restart_game` or initialization path using:

```c
Game_Rng_Seed(&g_rng, Game_Runtime_Get_Tick_Ms() ^ GAME_SEED_CONSTANT);
```

Use these exact distinct constants:

```c
/* snake */       0x51A9E37Du
/* tetris */      0x2E71B864u
/* pacman */      0x013579BDu
/* air_battle */  0x6D2B79F5u
/* game_2048 */   0x3C8BF915u
/* tank_battle */ 0x74A91C3Du
/* dodge_box */   0x4D595DF4u
```

For every `% bound` call use `Game_Rng_Range(&g_rng, bound)`. Replace bit choices such as `random_next() & 1u` with `Game_Rng_Range(&g_rng, 2u)`. Tetris's existing unbiased helper becomes direct `Game_Rng_Range(&g_rng, PIECE_COUNT)`.

- [ ] **Step 3: Verify private generators are gone and build**

Run:

```bash
! rg -n 'static uint32_t (random_next|rng_next)|g_random_state|g_rng_state' \
  src/app/games/{snake,tetris,pacman,air_battle,game_2048,tank_battle,dodge_box}
python3 scripts/cc.py --target vm
```

Expected: the scan has no output and VM build succeeds.

- [ ] **Step 4: Commit**

```bash
git add src/app/games/{snake,tetris,pacman,air_battle,game_2048,tank_battle,dodge_box}
git commit -m "refactor: migrate games to runtime rng"
```

### Task 3: Migrate the Alternate LCG Games

**Files:**
- Modify: `src/app/games/maze/maze.c`
- Modify: `src/app/games/dino_runner/dino_runner.c`
- Modify: `src/app/games/flappy_bird/flappy_bird.c`
- Modify: `src/app/games/rhythm/rhythm.c`

**Interfaces:**
- Consumes: the `Game_rng` API from Task 1.
- Produces: the remaining randomized games using unbiased bounded choices.

- [ ] **Step 1: Capture the failing alternate-generator scan**

```bash
rg -n 'static uint32_t fast_rand|g_seed|g_rand_state' \
  src/app/games/{maze,dino_runner,flappy_bird,rhythm}
```

Expected: private generator/state matches in all four sources.

- [ ] **Step 2: Replace each alternate generator**

Use `static Game_rng g_rng;`, seed in the existing reset path, and use `Game_Rng_Range` at every bounded call. Use these constants:

```c
/* maze */         0xA341316Cu
/* dino_runner */  0xC8013EA4u
/* flappy_bird */  0xAD90777Du
/* rhythm */       0x7E95761Eu
```

For Rhythm's direction selection, replace `1u + (fast_rand() & 3u)` with:

```c
g_notes[i].dir = (uint8_t)(1u + Game_Rng_Range(&g_rng, 4u));
```

- [ ] **Step 3: Run all RNG checks**

```bash
! rg -n 'static uint32_t (random_next|fast_rand|rng_next)|g_random_state|g_rand_state|g_rng_state' \
  src/app/games
/tmp/game_runtime_rng_test
python3 scripts/cc.py --target vm
```

Expected: no private generator matches, host RNG test exits 0, and VM build succeeds.

- [ ] **Step 4: Commit**

```bash
git add src/app/games/{maze,dino_runner,flappy_bird,rhythm}
git commit -m "refactor: unify remaining game random sources"
```

### Task 4: Return Outcomes from Winnable and Match Games

**Files:**
- Modify: `src/app/games/snake/snake.c`
- Modify: `src/app/games/tank_battle/tank_battle.c`
- Modify: `src/app/games/air_battle/air_battle.c`
- Modify: `src/app/games/breakout/breakout.c`
- Modify: `src/app/games/pong/pong.c`
- Modify: `src/app/games/gomoku/gomoku.c`
- Modify: `src/app/games/maze/maze.c`
- Modify: `src/app/games/dodge_box/dodge_box.c`

**Interfaces:**
- Consumes: `game_result_won` and `game_result_lost` from Task 1.
- Produces: exact-frame outcomes for every game that has both success/failure or a success terminal.

- [ ] **Step 1: Prove terminal outcomes are not yet returned**

```bash
rg -L 'game_result_won' src/app/games/{snake,tank_battle,air_battle,breakout,pong,gomoku,maze,dodge_box}/*.c
```

Expected: all eight sources are listed.

- [ ] **Step 2: Replace private terminal/replay branches with direct results**

At the top-of-update terminal guards and immediately after any helper that can change terminal state, return according to this exact mapping:

```c
/* Snake */       g_state == snake_state_win ? game_result_won : game_result_lost
/* Tank */        g_state == tank_state_win ? game_result_won : game_result_lost
/* Air Battle */  g_state == air_state_win ? game_result_won : game_result_lost
/* Breakout */    g_state == breakout_state_win ? game_result_won : game_result_lost
/* Pong */        g_player_score > g_ai_score ? game_result_won : game_result_lost
/* Gomoku */      g_winner == 1u ? game_result_won : game_result_lost
/* Maze */        game_result_won
/* Dodge Box */   g_state == game_state_clear ? game_result_won : game_result_lost
```

Remove terminal-only drawing, restart prompts, generic victory/defeat SFX, and terminal vibration from their transition helpers. Remove input-triggered `restart_game()` calls from terminal update branches, but retain each reset helper for `Init` and later console replay.

- [ ] **Step 3: Verify outcome tokens and build**

```bash
for game in snake tank_battle air_battle breakout pong gomoku maze dodge_box; do
  rg -q 'game_result_won' "src/app/games/$game/$game.c"
done
python3 scripts/cc.py --target vm
```

Expected: every scan succeeds and VM build completes.

- [ ] **Step 4: Commit**

```bash
git add src/app/games/{snake,tank_battle,air_battle,breakout,pong,gomoku,maze,dodge_box}
git commit -m "refactor: report explicit game outcomes"
```

### Task 5: Return Outcomes from Loss-only Games

**Files:**
- Modify: `src/app/games/dino_runner/dino_runner.c`
- Modify: `src/app/games/flappy_bird/flappy_bird.c`
- Modify: `src/app/games/tetris/tetris.c`
- Modify: `src/app/games/pacman/pacman.c`
- Modify: `src/app/games/rhythm/rhythm.c`
- Modify: `src/app/games/game_2048/game_2048.c`
- Modify: `src/app/games/needle/needle.c`

**Interfaces:**
- Consumes: `game_result_lost` from Task 1.
- Produces: exact-frame loss results while preserving Pacman's level-clear and 2048's tile-win continuation.

- [ ] **Step 1: Prove loss results are absent**

```bash
rg -L 'game_result_lost' src/app/games/{dino_runner,flappy_bird,tetris,pacman,rhythm,game_2048,needle}/*.c
```

Expected: all seven sources are listed.

- [ ] **Step 2: Return loss on the exact terminal transition**

Use this guard at an already-terminal update entry:

```c
if (g_state == terminal_state) { return game_result_lost; }
```

After collision, exhausted lives, failed spawn, or no-moves logic sets the terminal state, return `game_result_lost` immediately. Remove the terminal restart key, terminal prompt drawing, `buzzer_sfx_defeat`, terminal `buzzer_sfx_life_lost`, and `vib_effect_defeat` at those sites. Preserve non-terminal life-loss feedback.

Pacman must keep `game_state_level_clear` returning `game_result_running`; only `game_state_over` returns lost. 2048 must keep `state_win` running and return lost only when `!can_move()` sets `state_over`:

```c
if (has_2048() && g_state == state_playing) {
    g_state = state_win;
    render_hud();
}
if (!can_move()) {
    g_state = state_over;
    render_hud();
    return game_result_lost;
}
```

- [ ] **Step 3: Verify 2048 has no win result and build**

```bash
for game in dino_runner flappy_bird tetris pacman rhythm game_2048 needle; do
  rg -q 'game_result_lost' "src/app/games/$game/$game.c"
done
! rg -n 'game_result_won' src/app/games/game_2048/game_2048.c
python3 scripts/cc.py --target vm
```

Expected: all games contain a loss result, 2048 contains no win result, and VM build succeeds.

- [ ] **Step 4: Commit**

```bash
git add src/app/games/{dino_runner,flappy_bird,tetris,pacman,rhythm,game_2048,needle}
git commit -m "refactor: report terminal losses directly"
```

### Task 6: Transfer Terminal Ownership to the Console

**Files:**
- Create: `tests/app/game_contract_test.py`
- Modify: `src/app/game_console/game_console.c`
- Modify: `src/app/game_console/game_over_menu.h`
- Modify: `src/app/game_console/game_over_menu.c`
- Modify: `src/app/game_console/game_registry.h`
- Modify: `src/app/game_console/game_registry.c`
- Modify: every `.h` and `.c` under `src/app/games` that declares or defines `*_Is_Finished`

**Interfaces:**
- Consumes: terminal update results from Tasks 4–5.
- Produces: `Game_Over_Menu_Open(..., Game_result result, ..., uint32_t score)` and a registry without `is_finished`.

- [ ] **Step 1: Write the failing source-contract test**

Create `tests/app/game_contract_test.py`:

```python
import pathlib
import re
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]
GAMES = ROOT / "src/app/games"


class GameContractTest(unittest.TestCase):
    def test_finish_polling_is_removed(self):
        text = "\n".join(p.read_text() for p in (ROOT / "src/app").rglob("*.[ch]"))
        self.assertNotRegex(text, r"\b[A-Za-z0-9_]+_Is_Finished\b|\.is_finished\b")

    def test_games_do_not_own_generic_terminal_feedback(self):
        sources = [p for p in GAMES.rglob("*.c") if p.name != "pacman.c"]
        text = "\n".join(p.read_text() for p in sources)
        self.assertNotRegex(text, r"buzzer_sfx_(victory|defeat)|vib_effect_(victory|defeat)")
        pacman = (GAMES / "pacman/pacman.c").read_text()
        self.assertNotRegex(pacman, r"buzzer_sfx_defeat|vib_effect_defeat")

    def test_games_do_not_offer_terminal_restart_prompts(self):
        text = "\n".join(p.read_text() for p in GAMES.rglob("*.c"))
        self.assertNotRegex(text, r'"[^"\n]*RESTART[^"\n]*"')

    def test_tools_never_report_game_outcomes(self):
        tools = ["bad_apple", "calculator", "fps_test", "info", "sfx_lib", "volume_control"]
        for tool in tools:
            text = (GAMES / tool / f"{tool}.c").read_text()
            self.assertNotIn("game_result_won", text)
            self.assertNotIn("game_result_lost", text)


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Step 2: Run it and verify legacy ownership fails**

```bash
python3 -m unittest tests/app/game_contract_test.py -v
```

Expected: failures for finish polling and game-owned terminal feedback/prompts.

- [ ] **Step 3: Pass the outcome into the game-over component**

Change the declaration and definition to:

```c
void Game_Over_Menu_Open(St7789* lcd, Buzzer* buzzer, Vib_motor_gpio* vib_motor,
    Game_result result, uint8_t game_id, const char* game_name, uint32_t score);
```

Store `result`, render `YOU WIN` in green or `YOU LOSE` in red in `render_prompt`, and emit one terminal response in `Game_Over_Menu_Open`:

```c
if (result == game_result_won) {
    Buzzer_Play_Sfx(g_buzzer, buzzer_sfx_victory);
    Vib_Motor_Gpio_Play_Effect(g_vib_motor, vib_effect_victory);
} else {
    Buzzer_Play_Sfx(g_buzzer, buzzer_sfx_defeat);
    Vib_Motor_Gpio_Play_Effect(g_vib_motor, vib_effect_defeat);
}
```

- [ ] **Step 4: Route update results directly in the console**

Replace finish polling in `update_active_game` with:

```c
const Game_result result = game->update(input);
if (result == game_result_exit) { return result; }
if (result == game_result_won || result == game_result_lost) {
    g_console_state = console_state_game_over;
    Game_Over_Menu_Open(g_lcd, g_buzzer, g_vib_motor, result, game->id, game->name, game->get_score());
}
return game_result_running;
```

- [ ] **Step 5: Remove the obsolete registry and game APIs**

Delete `uint8_t (*is_finished)(void);` from `Game_descriptor`, every `.is_finished = ...` initializer, and every `*_Is_Finished` declaration and definition in both games and tools. Do not change tool update return values.

- [ ] **Step 6: Run the contract test, scans, and VM build**

```bash
python3 -m unittest tests/app/game_contract_test.py -v
! rg -n 'Is_Finished|\.is_finished' src/app
python3 scripts/cc.py --target vm
```

Expected: four contract tests pass, the finish-polling scan has no output, and VM build succeeds. Pacman's non-terminal level-clear victory cue remains allowed.

- [ ] **Step 7: Commit**

```bash
git add tests/app/game_contract_test.py src/app/game_console src/app/games
git commit -m "refactor: centralize game completion in console"
```

### Task 7: Update APP Documentation and Run Full Verification

**Files:**
- Modify: `docs/modules.md`
- Modify: `docs/developer_guide.md`
- Modify: `docs/freertos_design.md`

**Interfaces:**
- Consumes: final lifecycle and RNG interfaces.
- Produces: contributor documentation matching the implemented APP contract.

- [ ] **Step 1: Confirm documentation is stale**

```bash
rg -n 'is_finished|Xxx_Is_Finished' docs/modules.md docs/developer_guide.md docs/freertos_design.md
```

Expected: the three current references are printed.

- [ ] **Step 2: Update the registry and game-authoring documentation**

In `docs/modules.md`, remove `is_finished` from the descriptor example and add:

```c
typedef enum {
    game_result_running,
    game_result_exit,
    game_result_won,
    game_result_lost,
} Game_result;
```

Explain that `update` owns the transition report while the console owns terminal feedback, score entry, leaderboard, and replay. Document the `Game_rng` signatures and per-game seeding rule.

In `docs/developer_guide.md`, remove `Xxx_Is_Finished`, state that terminal paths return won/lost immediately, tools use only running/exit, and instruct randomized games to keep a private `Game_rng` and call `Game_Rng_Range` instead of defining a PRNG.

In `docs/freertos_design.md`, change `init/update/get_score/is_finished` to `init/update/get_score` and state that the Game task dispatches `Game_result` into pause/menu/game-over flow.

- [ ] **Step 3: Format modified C files**

```bash
source scripts/format.bash
```

Expected: formatter completes without error; inspect `git diff` to ensure it did not touch unrelated files.

- [ ] **Step 4: Run the complete verification suite**

```bash
cc -std=c99 -Wall -Wextra -Werror \
  -Isrc/app/game_console -Isrc/hal/buzzer -Isrc/hal/st7789 \
  -Isrc/hal/vib_motor_gpio -Isrc/bsp/time \
  tests/app/game_runtime_rng_test.c src/app/game_console/game_runtime.c \
  -o /tmp/game_runtime_rng_test
/tmp/game_runtime_rng_test
python3 -m unittest tests/app/game_contract_test.py -v
! rg -n 'Is_Finished|\.is_finished' src/app docs/modules.md docs/developer_guide.md docs/freertos_design.md
! rg -n 'static uint32_t (random_next|fast_rand|rng_next)|g_random_state|g_rand_state|g_rng_state' src/app/games
git diff --check
python3 scripts/cc.py --target vm
python3 scripts/cc.py --target arm
```

Expected: host tests pass, all negative scans have no output, diff check is clean, and both builds complete successfully.

- [ ] **Step 5: Review the final diff against the outcome table**

Inspect every terminal assignment in the fifteen game sources and verify: Snake/Tank/Air/Breakout/Pong/Gomoku/Maze/Dodge can report won as specified; all approved failure paths report lost; Pacman level clear and 2048 tile win remain running; tools contain only running/exit.

- [ ] **Step 6: Commit documentation and any formatter-only adjustments**

```bash
git add docs/modules.md docs/developer_guide.md docs/freertos_design.md src/app
git commit -m "docs: describe unified game lifecycle and rng"
```

- [ ] **Step 7: Confirm the branch is ready for review**

```bash
git status --short --branch
git log --oneline --decorate --max-count=8
```

Expected: clean `feature/unify-game-lifecycle-rng` worktree with the design commit and seven implementation commits.
