# Game Control Hints Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Show each game's actual A/Y controls beside `X/B PAUSE` in the non-paused bottom status bar.

**Architecture:** Store the game-specific control fragment in `Game_descriptor`, format the complete bottom-bar text in a small pure C helper, and route every non-paused game redraw through one console helper. Keep pause and non-game application text unchanged.

**Tech Stack:** C99, existing game registry and graphics APIs, host `cc` test executable, CMake VM build.

## Global Constraints

- Games without a Y action do not show Y or a placeholder.
- Games with no active A/Y operation show only `X/B PAUSE`.
- Pause remains `A/X RESUME B MENU`.
- Non-game applications remain `A OK  B BACK`.
- The formatted hint is at most 28 visible characters so it ends before the FPS region.
- Do not change input behavior or game rules.

---

### Task 1: Pure control-hint formatter

**Files:**
- Create: `src/app/game_console/game_control_hint.h`
- Create: `src/app/game_console/game_control_hint.c`
- Create: `tests/game_control_hint_test.c`

**Interfaces:**
- Consumes: a nullable game-specific control fragment and `is_game` flag.
- Produces: `GAME_CONTROL_HINT_TEXT_MAX` and `Game_Control_Hint_Format(const char*, uint8_t, char[GAME_CONTROL_HINT_TEXT_MAX])`.

- [ ] **Step 1: Write the failing formatter test**

```c
#include <assert.h>
#include <string.h>

#include "game_control_hint.h"

static void expect_hint(const char* controls, uint8_t is_game, const char* expected) {
    char output[GAME_CONTROL_HINT_TEXT_MAX];
    Game_Control_Hint_Format(controls, is_game, output);
    assert(strcmp(output, expected) == 0);
    assert(strlen(output) <= GAME_CONTROL_HINT_VISIBLE_MAX);
}

int main(void) {
    expect_hint("A FIRE", 1u, "A FIRE  X/B PAUSE");
    expect_hint("A ROTATE Y DROP", 1u, "A ROTATE Y DROP  X/B PAUSE");
    expect_hint(NULL, 1u, "X/B PAUSE");
    expect_hint("", 1u, "X/B PAUSE");
    expect_hint(NULL, 0u, "A OK  B BACK");
    return 0;
}
```

- [ ] **Step 2: Run the test and verify RED**

Run:

```bash
cc -std=c99 -Wall -Wextra -Werror -Isrc/app/game_console \
  tests/game_control_hint_test.c src/app/game_console/game_control_hint.c \
  -o /tmp/game_control_hint_test
```

Expected: compilation fails because `game_control_hint.h` and the formatter do not exist.

- [ ] **Step 3: Add the minimal formatter**

Create `game_control_hint.h`:

```c
#pragma once

#include <stdint.h>

#define GAME_CONTROL_HINT_VISIBLE_MAX 28u
#define GAME_CONTROL_HINT_TEXT_MAX    (GAME_CONTROL_HINT_VISIBLE_MAX + 1u)

void Game_Control_Hint_Format(
    const char* controls, uint8_t is_game, char output[GAME_CONTROL_HINT_TEXT_MAX]);
```

Create `game_control_hint.c`:

```c
#include "game_control_hint.h"

#include <stddef.h>

static void append_text(char** cursor, uint8_t* remaining, const char* text) {
    while (*text != '\0' && *remaining > 1u) {
        **cursor = *text;
        (*cursor)++;
        text++;
        (*remaining)--;
    }
    **cursor = '\0';
}

void Game_Control_Hint_Format(
    const char* controls, uint8_t is_game, char output[GAME_CONTROL_HINT_TEXT_MAX]) {
    char* cursor = output;
    uint8_t remaining = GAME_CONTROL_HINT_TEXT_MAX;
    output[0] = '\0';

    if (!is_game) {
        append_text(&cursor, &remaining, "A OK  B BACK");
        return;
    }
    if (controls != NULL && controls[0] != '\0') {
        append_text(&cursor, &remaining, controls);
        append_text(&cursor, &remaining, "  ");
    }
    append_text(&cursor, &remaining, "X/B PAUSE");
}
```

- [ ] **Step 4: Run the formatter test and verify GREEN**

Run:

```bash
cc -std=c99 -Wall -Wextra -Werror -Isrc/app/game_console \
  tests/game_control_hint_test.c src/app/game_console/game_control_hint.c \
  -o /tmp/game_control_hint_test && /tmp/game_control_hint_test
```

Expected: exit status 0 with no output.

- [ ] **Step 5: Commit the formatter**

```bash
git add src/app/game_console/game_control_hint.h \
  src/app/game_console/game_control_hint.c tests/game_control_hint_test.c
git commit -m "feat: add game control hint formatter"
```

---

### Task 2: Register each game's A/Y control fragment

**Files:**
- Modify: `src/app/game_console/game_registry.h:55-65`
- Modify: `src/app/game_console/game_registry.c:25-215`
- Create: `tests/game_registry_control_hints_test.py`

**Interfaces:**
- Consumes: `Game_descriptor` initializers in the registry.
- Produces: nullable `const char* control_hint` on every descriptor; `NULL` means no active A/Y operation.

- [ ] **Step 1: Write the failing registry mapping test**

Create `tests/game_registry_control_hints_test.py`:

```python
import re
import unittest
from pathlib import Path


class GameRegistryControlHintsTest(unittest.TestCase):
    def test_every_game_has_the_expected_control_hint(self):
        source = Path("src/app/game_console/game_registry.c").read_text()
        entries = re.findall(r"\{\s*(\.name\s*=.*?\n\s*)\},", source, re.S)
        actual = {}
        for entry in entries:
            name = re.search(r'\.name\s*=\s*"([^"]+)"', entry).group(1)
            if not re.search(r"\.is_game\s*=\s*1", entry):
                continue
            hint = re.search(r'\.control_hint\s*=\s*(NULL|"[^"]*")', entry)
            self.assertIsNotNone(hint, name)
            actual[name] = None if hint.group(1) == "NULL" else hint.group(1)[1:-1]

        self.assertEqual(actual, {
            "PAC-MAN": None,
            "SNAKE": None,
            "TANK": "A FIRE",
            "AIR FORCE": "A BOMB",
            "TETRIS": "A ROTATE Y DROP",
            "BREAKOUT": "A LAUNCH",
            "PONG": "A SERVE",
            "GOMOKU": "A PLACE",
            "2048": None,
            "DINO": "A JUMP Y DUCK",
            "FLAPPY": "A FLAP Y GLIDE",
            "MAZE": None,
            "NEEDLE": "A LAUNCH Y QUICK",
            "DODGE": None,
            "RHYTHM": "A START",
        })


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Step 2: Run the test and verify RED**

Run:

```bash
python3 tests/game_registry_control_hints_test.py
```

Expected: FAIL because game entries do not contain `.control_hint`.

- [ ] **Step 3: Add the descriptor field and exact registry mappings**

Add this field after `id` in `Game_descriptor`:

```c
const char* control_hint;
```

Add the following designated initializer to each `g_games` entry:

```c
/* PAC-MAN */   .control_hint = NULL,
/* SNAKE */     .control_hint = NULL,
/* TANK */      .control_hint = "A FIRE",
/* AIR FORCE */ .control_hint = "A BOMB",
/* TETRIS */    .control_hint = "A ROTATE Y DROP",
/* BREAKOUT */  .control_hint = "A LAUNCH",
/* PONG */      .control_hint = "A SERVE",
/* GOMOKU */    .control_hint = "A PLACE",
/* 2048 */      .control_hint = NULL,
/* DINO */      .control_hint = "A JUMP Y DUCK",
/* FLAPPY */    .control_hint = "A FLAP Y GLIDE",
/* MAZE */      .control_hint = NULL,
/* NEEDLE */    .control_hint = "A LAUNCH Y QUICK",
/* DODGE */     .control_hint = NULL,
/* RHYTHM */    .control_hint = "A START",
```

Non-game SFX, CALC, FPS TEST, VOLUME, and INFO entries use `.control_hint = NULL`.

- [ ] **Step 4: Run the registry and formatter tests**

Run:

```bash
python3 tests/game_registry_control_hints_test.py
/tmp/game_control_hint_test
```

Expected: the Python test reports `OK`; the formatter test exits 0 with no output.

- [ ] **Step 5: Commit the registry mapping**

```bash
git add src/app/game_console/game_registry.h \
  src/app/game_console/game_registry.c tests/game_registry_control_hints_test.py
git commit -m "feat: register game control hints"
```

---

### Task 3: Route all active-game bottom bars through the formatter

**Files:**
- Modify: `src/app/game_console/game_console.c:1-20,750-830,920-940`

**Interfaces:**
- Consumes: `Game_descriptor.control_hint` and `Game_Control_Hint_Format`.
- Produces: one `draw_running_bottom_bar(const Game_descriptor*)` path for entry, resume, replay, and screensaver restoration.

- [ ] **Step 1: Add a source-level regression check and verify RED**

Run:

```bash
! rg -q 'game->is_game \? "X/B PAUSE" : "A OK  B BACK"' \
  src/app/game_console/game_console.c
```

Expected: command fails because the duplicated old expression is still present.

- [ ] **Step 2: Add the centralized draw helper**

Include the formatter header and add a reusable buffer/helper near the other console statics:

```c
#include "game_control_hint.h"

static char g_running_bottom_hint[GAME_CONTROL_HINT_TEXT_MAX];

static void draw_running_bottom_bar(const Game_descriptor* game) {
    if (game == NULL) { return; }
    Game_Control_Hint_Format(game->control_hint, game->is_game, g_running_bottom_hint);
    Game_Graphics_Draw_Bottom_Bar(g_lcd, g_running_bottom_hint, g_current_fps);
}
```

- [ ] **Step 3: Replace every duplicated active-state draw call**

Replace the bottom-bar calls after initial `game->init`, pause resume, replay `game->init`, and screensaver restoration with:

```c
draw_running_bottom_bar(game);
```

Do not change `Game_Graphics_Draw_Pause_Bottom_Bar`, menu rendering, or Game Over rendering.

- [ ] **Step 4: Run focused checks and the host test**

Run:

```bash
! rg -q 'game->is_game \? "X/B PAUSE" : "A OK  B BACK"' \
  src/app/game_console/game_console.c
test "$(rg -c 'draw_running_bottom_bar\(game\)' \
  src/app/game_console/game_console.c)" -eq 4
rg -q 'Game_Graphics_Draw_Bottom_Bar\(lcd, "A/X RESUME B MENU", fps\)' \
  src/app/game_console/game_graphics.c
python3 tests/game_registry_control_hints_test.py
cc -std=c99 -Wall -Wextra -Werror \
  -Isrc/app/game_console \
  tests/game_control_hint_test.c src/app/game_console/game_control_hint.c \
  -o /tmp/game_control_hint_test && /tmp/game_control_hint_test
```

Expected: every command exits 0.

- [ ] **Step 5: Build the VM target**

Run:

```bash
python3 scripts/cc.py --target vm
```

Expected: `framework_vm` builds successfully with no compiler errors.

- [ ] **Step 6: Inspect the final diff and commit**

Run:

```bash
git diff --check
git diff --stat HEAD~2
git status --short
```

Expected: no whitespace errors; only the planned formatter, registry, console, and test files are changed.

```bash
git add src/app/game_console/game_console.c
git commit -m "feat: show per-game controls in status bar"
```
