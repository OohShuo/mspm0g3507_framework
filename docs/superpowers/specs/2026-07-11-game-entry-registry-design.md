# Game Entry Registry Design

Date: 2026-07-11

## Goal

Adopt the X-macro registration pattern used by `infantry-2026/APP/motion` for every game-console entry. Each game or tool will be self-contained in one primary `.c` file: implementation, metadata, menu icon, and its public descriptor live together. The central console will know only the common descriptor interface and registration list.

Resource-specific support files remain allowed. In particular, `air_assets.c/.h`, Info image headers, and video/resource modules are outside the one-primary-`.c` rule.

## Registration List

Create `src/app/game_console/game_entries.inc` as the single ordered list of all console entries:

```c
#ifndef game_entry
#define game_entry(name)
#endif

game_entry(pacman)
game_entry(snake)
game_entry(tank_battle)
game_entry(air_battle)
game_entry(tetris)
game_entry(breakout)
game_entry(pong)
game_entry(gomoku)
game_entry(game_2048)
game_entry(dino_runner)
game_entry(flappy_bird)
game_entry(maze)
game_entry(needle)
game_entry(dodge_box)
game_entry(rhythm)
game_entry(sfx_lib)
game_entry(calculator)
game_entry(fps_test)
game_entry(volume_control)
game_entry(bad_apple)
game_entry(info)
```

The list contains tokens only. Names, help text, function pointers, colors, and game/tool classification do not belong in the `.inc` file. This keeps each entry's product definition in its own source file and makes adding an entry a two-step operation: add one `.c`, then add one ordered token.

The existing `g_games[]` order of all 21 registered entries becomes the canonical order. The unregistered `racing` placeholder is deleted. Numeric IDs are regenerated as a contiguous range from 0 through 20, and persisted score data is migrated as described below.

## Generated Interfaces

`game_registry.h` includes `game_entries.inc` to generate the ID enum:

```c
typedef enum {
#define game_entry(name) game_id_##name,
#include "game_entries.inc"
#undef game_entry
    game_id_count,
} Game_id;
```

Tokens follow primary source basenames. The resulting internal identifiers use names such as `game_id_tank_battle`, `game_id_air_battle`, `game_id_game_2048`, and `game_id_dino_runner`. Their values equal their positions in the current menu registration order. `game_id_count` becomes 21.

`game_registry.c` includes the same list twice: once to declare each exported descriptor and once to create the ordered pointer array.

```c
#define game_entry(name) extern const Game_descriptor game_##name##_entry;
#include "game_entries.inc"
#undef game_entry

static const Game_descriptor* const g_games[] = {
#define game_entry(name) &game_##name##_entry,
#include "game_entries.inc"
#undef game_entry
};
```

`Game_Registry_Get()` returns the pointer already stored in the array. `Game_Registry_Count()` remains derived from the array size and is checked against `game_id_count` at compile time. The descriptor's `.id` must equal its array position.

## Score Store Migration

Increment `SCORE_STORE_VERSION` from 3 to 4. Version 4 uses the new contiguous `.inc` order. When a valid version-3 score file is loaded, copy complete leaderboard lists with this mapping:

| Entry | Old ID | New ID |
| --- | ---: | ---: |
| Pacman | 0 | 0 |
| Snake | 1 | 1 |
| Tank Battle | 3 | 2 |
| Air Battle | 4 | 3 |
| Tetris | 5 | 4 |
| Breakout | 6 | 5 |
| Pong | 7 | 6 |
| Gomoku | 8 | 7 |
| 2048 | 9 | 8 |
| Dino Runner | 10 | 9 |
| Flappy Bird | 11 | 10 |
| Maze | 12 | 11 |
| Needle | 13 | 12 |
| Dodge Box | 19 | 13 |
| Rhythm | 20 | 14 |
| SFX | 17 | 15 |
| Calculator | 14 | 16 |
| FPS Test | 15 | 17 |
| Volume | 18 | 18 |
| Bad Apple | 21 | 19 |
| Info | 16 | 20 |

Old slot 2 (`racing`) is discarded. Migration preserves each entry's count, names, scores, and ordering, writes version 4, marks the store dirty, and commits it through the existing save path. A version-1 file is first interpreted in the legacy ID layout and then mapped into the same version-4 order. Invalid checksums or unsupported versions continue to reset the store through the existing recovery behavior.

## Self-contained Entry Sources

Every game and tool primary `.c` includes `game_registry.h` and defines its entry-facing functions as file-local functions:

```c
static void pacman_init(const Game_hardware* hardware);
static Game_result pacman_update(const Game_input* input);
static uint32_t pacman_get_score(void);
static void pacman_draw_icon(St7789* lcd, int32_t x, int32_t y);

const Game_descriptor game_pacman_entry = {
    .name = "PAC-MAN",
    .id = game_id_pacman,
    .control_hint = NULL,
    .info_text = "DESCRIPTION\nClassic maze chase.",
    .draw_icon = pacman_draw_icon,
    .name_color = 0xffe0u,
    .is_game = 1,
    .init = pacman_init,
    .update = pacman_update,
    .get_score = pacman_get_score,
};
```

Only `game_<token>_entry` has external linkage. The old public init/update/get-score names and the 21 ordinary per-entry headers are removed. Resource support headers are retained when another `.c` file genuinely provides a separate resource interface.

All 15 games and the six tools use the same pattern. Tools keep `is_game = 0` and continue to return only running/exit lifecycle results.

## Icon Interface

Remove `Game_icon` and the descriptor's icon enum field. Add:

```c
typedef void (*Game_draw_icon)(St7789* lcd, int32_t x, int32_t y);

typedef struct {
    const char* name;
    Game_id id;
    const char* control_hint;
    const char* info_text;
    Game_draw_icon draw_icon;
    uint16_t name_color;
    void (*init)(const Game_hardware* hardware);
    Game_result (*update)(const Game_input* input);
    uint32_t (*get_score)(void);
    uint8_t is_game;
} Game_descriptor;
```

The `x/y` arguments identify the upper-left corner of a 48×40 icon canvas inside a menu cell. Each entry draws entirely inside that canvas and receives the LCD explicitly; it must not depend on console-private global state.

Existing icon drawing code moves from `game_console.c` into the corresponding entry source and is translated to canvas-local coordinates without changing its appearance. `name_color` preserves the current unselected label colors: Pacman is yellow, Snake is green, and all other entries are cyan. Selected labels remain white under console control.

`draw_grid_cell()` becomes generic:

```c
if (game->draw_icon != NULL) {
    game->draw_icon(g_lcd, icon_x, icon_y);
}
```

The central console retains cell background, border, label, high score, pagination, and selection rendering. It contains no entry-specific icon functions, enum comparisons, or fallback icon switch.

## File Changes

- Create `src/app/game_console/game_entries.inc`.
- Modify `game_registry.h/.c` to generate IDs and the pointer array.
- Modify `game_console.c` to call `draw_icon` and remove all entry-specific icon code.
- Modify all 21 primary sources under `src/app/games` to own metadata, icon drawing, and descriptor definition.
- Delete the 21 ordinary per-entry headers.
- Preserve `air_assets.c/.h`, Info image headers, and other separate resource modules.
- Update APP architecture and developer documentation to describe the X-macro entry workflow.

The existing recursive APP CMake glob already discovers every primary `.c`; no per-game build-list maintenance is introduced.

## Validation and Error Handling

Registration errors are build-time failures rather than runtime fallbacks:

- A token without `game_<token>_entry` fails at link time.
- A duplicate token fails through duplicate enum/symbol initialization.
- Array length is compile-time checked against `game_id_count`.
- Registry validation asserts that each descriptor ID equals its generated array index.
- A descriptor may use `draw_icon = NULL`; the console then renders the rest of the cell without an icon. All built-in entries provide a non-null icon.
- `Game_Registry_Get()` continues returning `NULL` for an out-of-range index.

## Testing

Add or extend host-side contract tests to verify:

- `game_entries.inc` contains the expected 21 tokens in the preserved order;
- the registry enum, extern declarations, and array are generated from the `.inc` file;
- every listed token has exactly one `game_<token>_entry` definition;
- every descriptor has an ID, icon callback, lifecycle callbacks, score callback, and correct `is_game` classification;
- ordinary per-entry headers are absent while approved resource headers remain;
- `game_console.c` contains no `game_icon_`, entry-specific `draw_*_icon`, or icon dispatch chain;
- version-3 and version-1 score fixtures migrate every legacy slot to the new contiguous ID, with old racing data discarded;
- VM and ARM builds succeed without warnings.

Manual VM inspection confirms that all 21 menu icons, label colors, selection borders, high scores, pagination, and entry launch behavior remain visually and functionally unchanged.

## Non-goals

- Changing game rules, lifecycle results, controls, scores, or replay behavior.
- Folding large generated image/video data or reusable resource providers into primary game sources.
- Replacing X-macros with linker-section discovery.
- Reordering menu entries after the current 21-entry order becomes canonical.
- Redesigning menu visuals.
