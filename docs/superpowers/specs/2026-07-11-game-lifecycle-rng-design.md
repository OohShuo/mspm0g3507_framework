# Unified Game Lifecycle and RNG Design

Date: 2026-07-11

## Goal

Remove duplicated end-of-game and pseudo-random-number infrastructure from APP games. The game console will own terminal outcome feedback, score submission, leaderboard display, and replay. Each game will report its lifecycle result directly from its update function and keep only game-specific state and presentation. Randomized games will use an explicit per-game RNG supplied by `game_runtime` instead of private LCG implementations.

This change is limited to `src/app`. It does not redesign game rules, scoring, pause behavior, or hardware abstraction.

## Current Problems

The console currently calls a game's `update` function and then separately polls `is_finished`. Individual games also implement their own terminal messages, victory/defeat sound or vibration, and restart handling even though the console immediately opens the shared game-over menu. This creates two competing owners for the same lifecycle transition.

Eleven games also contain private LCG variants. They duplicate seeding, range reduction, and state management, and several use biased modulo reduction.

## Lifecycle Contract

`game_runtime.h` will define the complete update result:

```c
typedef enum {
    game_result_running,
    game_result_exit,
    game_result_won,
    game_result_lost,
} Game_result;
```

Every game and tool update function returns one of these values. A terminal game returns `game_result_won` or `game_result_lost` on the exact update that enters its terminal state. It must stop further input and simulation work for that frame. Tools continue to use only `game_result_running` and `game_result_exit`.

`Game_descriptor` keeps `init`, `update`, `get_score`, and `is_game`. It removes `is_finished`. All public `*_Is_Finished` declarations and implementations are removed.

The console handles results as follows:

- `game_result_running`: keep the active game or tool running.
- `game_result_exit`: return to the console menu through the existing exit path.
- `game_result_won` or `game_result_lost`: read the score, open the shared game-over flow with the outcome, and switch to the game-over console state.

The shared game-over component accepts the terminal `Game_result`. It emits the generic victory or defeat sound and vibration exactly once, then owns score/name entry, leaderboard presentation, and the replay/menu choice. Replay calls the selected game's `init` function and returns to the active-game state.

Games no longer display terminal win/loss/restart prompts, play generic terminal victory/defeat feedback, or accept a private replay key. In-game feedback remains local to a game when play continues, including movement, hits, scoring, actions, and non-terminal life loss. A feedback event that coincides with immediate terminal transition is replaced by the console's single terminal feedback rather than producing both effects.

## Outcome Mapping

The following mappings define player outcomes and preserve current game rules:

| Game | Terminal condition | Result |
| --- | --- | --- |
| Snake | board/length win | won |
| Snake | collision game over | lost |
| Tank Battle | victory state | won |
| Tank Battle | game-over state | lost |
| Air Battle | victory state | won |
| Air Battle | game-over state | lost |
| Breakout | cleared state | won |
| Breakout | game-over state | lost |
| Pong | terminal score with player ahead | won |
| Pong | terminal score with AI ahead | lost |
| Gomoku | player is recorded winner | won |
| Gomoku | AI is recorded winner | lost |
| Maze | exit reached | won |
| Dodge Box | clear state | won |
| Dodge Box | failed state | lost |
| Dino Runner | game-over state | lost |
| Flappy Bird | game-over state | lost |
| Tetris | game-over state | lost |
| Pacman | lives exhausted | lost |
| Rhythm | game-over state | lost |
| Needle | game-over state | lost |
| 2048 | no legal moves / game-over state | lost |

Reaching the 2048 tile remains a non-terminal in-game state, matching current behavior; it does not return `game_result_won` as part of this refactor.

Maze's terminal goal, defeat, and terminal vibration effects are removed from the game implementation. The shared console emits one generic victory response. Other game-specific effects are treated by the same rule: terminal-only cues move to the console, while cues for events that allow continued play remain in the game.

## RNG Contract

`game_runtime.h` will expose an explicit state object and three functions:

```c
typedef struct {
    uint32_t state;
} Game_rng;

void Game_Rng_Seed(Game_rng *rng, uint32_t seed);
uint32_t Game_Rng_Next(Game_rng *rng);
uint32_t Game_Rng_Range(Game_rng *rng, uint32_t upper_bound);
```

`Game_Rng_Seed` replaces a zero seed with `0x6D2B79F5`. `Game_Rng_Next` applies the unsigned 32-bit transition `state = state * 1664525u + 1013904223u`, with wraparound defined by `uint32_t`. `Game_Rng_Range` uses rejection sampling to return an unbiased value in `[0, upper_bound)`: values below `(-upper_bound) % upper_bound` are discarded before applying modulo. `upper_bound == 0` returns zero defensively.

Each randomized game owns a `Game_rng` field in its private game state. Its `init` function seeds that field with `Game_Runtime_Get_Tick_Ms() ^ game_seed_constant`, where each game has a distinct nonzero constant. The zero-seed rule still applies to the mixed result. There is no global RNG state. Replay therefore reinitializes the game and receives a fresh seed without coupling random sequences between games.

The following private generators are replaced: Snake, Tetris, Pacman, Air Battle, 2048, Tank Battle, Maze, Dodge Box, Dino Runner, Flappy Bird, and Rhythm. Call sites use `Game_Rng_Next` only when the full word is needed and `Game_Rng_Range` for bounded choices.

Changing the generated sequences is an accepted consequence of unifying the algorithms. No promise of replay-identical random sequences is made.

## Interface and File Changes

The implementation will primarily affect:

- `src/app/game_console/game_runtime.h` and `.c`: lifecycle result and RNG API.
- `src/app/game_console/game_registry.h` and `.c`: remove `is_finished` registration.
- `src/app/game_console/game_console.c`: consume update results directly and route terminal results to the shared game-over flow.
- `src/app/game_console/game_over_menu.h` and `.c`: accept the terminal outcome and emit terminal feedback once.
- Game headers and sources under `src/app/games`: remove finish polling/replay ownership, return explicit outcomes, and migrate private RNG state.

No public BSP interface changes are required.

## Verification Strategy

Implementation will be test-driven where host-testable behavior is introduced.

Host-side RNG tests will verify:

- zero-seed normalization and deterministic seeding;
- the documented first values for a known seed;
- independent state between two `Game_rng` objects;
- range results are always below the bound;
- `upper_bound` values including zero, one, powers of two, and non-powers of two;
- rejection-path behavior using a deterministic state/sequence where practical.

Lifecycle and integration verification will include:

- compile-time/API checks proving descriptors no longer expose `is_finished`;
- checks that every game update returns the approved result for each terminal branch;
- checks that tools remain restricted to running/exit behavior;
- source scans showing no `*_Is_Finished`, descriptor `.is_finished`, private LCG helper, private terminal replay prompt, or game-owned generic terminal feedback remains;
- successful VM build;
- successful ARM build.

Manual review will confirm the game-over menu receives the score and exact outcome, terminal feedback is emitted once, replay reinitializes the selected game, and pause/menu behavior is unchanged.

## Non-Goals

- Changing gameplay balance, scoring, rendering, or control mappings.
- Adding deterministic replay or persisted RNG seeds.
- Treating the 2048 tile as an immediate victory.
- Moving ordinary in-game effects or UI into the console.
- Refactoring non-APP modules.

## Risks and Mitigations

- A terminal branch could still return `running`. The outcome table and per-branch verification make each transition explicit.
- A game could emit duplicate terminal feedback. Terminal effects are removed at the transition sites, and the console emits feedback only when opening the game-over flow.
- RNG migration can introduce off-by-one bounds. Each call site will retain its original intended interval and use the bounded API, backed by range tests.
- Time-only seeds can coincide. Mixing in a per-game constant separates games initialized at the same tick; cryptographic uniqueness is not required.
