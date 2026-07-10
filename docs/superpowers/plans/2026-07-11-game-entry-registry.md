# Game Entry Registry Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the centralized game registry and icon switch with a motion-style X-macro list whose 21 entries are self-contained in one primary `.c` each.

**Architecture:** `game_entries.inc` generates contiguous IDs, descriptor declarations, and the ordered registry pointer array. Each game/tool source exports one `game_<token>_entry` descriptor and keeps its implementation, metadata, and icon callback private. Score storage moves to version 4 and remaps legacy IDs before the generated ID order becomes active.

**Tech Stack:** C99, preprocessor X-macros, existing APP/HAL graphics APIs, Python 3 `unittest` contract tests, host C migration test, existing CMake VM and ARM builds.

## Global Constraints

- Preserve the current 21-entry menu order exactly; remove unregistered `racing`.
- Generate contiguous `Game_id` values 0–20 from `game_entries.inc`.
- Migrate valid score-store versions 1 and 3 into version 4; discard only legacy racing slot 2.
- Preserve all game rules, controls, lifecycle results, scores, menu visuals, label colors, pagination, and replay behavior.
- Each game/tool has one primary `.c`; resource providers such as `air_assets.c/.h`, Info image headers, and video modules remain separate.
- Only `game_<token>_entry` has external linkage from a primary entry source.
- Keep intermediate commits VM-buildable.

---

### Task 1: Add Tested Legacy Score-ID Migration

**Files:**
- Create: `tests/app/score_store_id_migration_test.c`
- Modify: `src/app/game_console/score_store.h`
- Modify: `src/app/game_console/score_store.c`

**Interfaces:**
- Produces: `uint8_t Score_Store_Map_Legacy_Id(uint8_t legacy_id)` returning the version-4 ID or `0xffu`.
- Produces: `Score_Store_Migrate_Legacy_Entries(uint8_t, const uint8_t[24], const Score_entry[24][10], uint8_t, uint8_t[24], Score_entry[24][10])`, the array-copy helper used by version-3 loading and host fixtures.
- Produces: score-store version 4 loading and migration used before Task 2 changes IDs.

- [ ] **Step 1: Write the failing legacy-ID unit test**

Create `tests/app/score_store_id_migration_test.c`:

```c
#include <assert.h>
#include <stdint.h>

#include "score_store.h"

int main(void) {
    static const uint8_t expected[] = {
        0u, 1u, 0xffu, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u,
        10u, 11u, 12u, 16u, 17u, 20u, 15u, 18u, 13u, 14u, 19u,
    };
    for (uint8_t old_id = 0; old_id < sizeof(expected); old_id++) {
        assert(Score_Store_Map_Legacy_Id(old_id) == expected[old_id]);
    }
    assert(Score_Store_Map_Legacy_Id((uint8_t)sizeof(expected)) == 0xffu);
    assert(Score_Store_Map_Legacy_Id(0xffu) == 0xffu);

    uint8_t old_counts[SCORE_STORE_MAX_GAMES] = {0};
    Score_entry old_entries[SCORE_STORE_MAX_GAMES][SCORE_STORE_TOP_COUNT] = {0};
    uint8_t new_counts[SCORE_STORE_MAX_GAMES] = {0};
    Score_entry new_entries[SCORE_STORE_MAX_GAMES][SCORE_STORE_TOP_COUNT] = {0};
    for (uint8_t old_id = 0; old_id < sizeof(expected); old_id++) {
        old_counts[old_id] = 1u;
        old_entries[old_id][0].score = 1000u + old_id;
        old_entries[old_id][0].name[0] = (char)('A' + old_id);
    }
    Score_Store_Migrate_Legacy_Entries(
        (uint8_t)sizeof(expected), old_counts, old_entries, 21u, new_counts, new_entries);
    for (uint8_t old_id = 0; old_id < sizeof(expected); old_id++) {
        const uint8_t new_id = expected[old_id];
        if (new_id == 0xffu) { continue; }
        assert(new_counts[new_id] == 1u);
        assert(new_entries[new_id][0].score == 1000u + old_id);
        assert(new_entries[new_id][0].name[0] == (char)('A' + old_id));
    }
    return 0;
}
```

- [ ] **Step 2: Compile and observe the missing API**

```bash
cc -std=c99 -Wall -Wextra -Werror -DFRAMEWORK_USE_LFS=0 \
  -Isrc/app/game_console -Isrc/vm/syscall \
  tests/app/score_store_id_migration_test.c src/app/game_console/score_store.c \
  -o /tmp/score_store_id_migration_test
```

Expected: compilation fails because `Score_Store_Map_Legacy_Id` is undeclared.

- [ ] **Step 3: Add the mapping API and version-4 migration**

Move `SCORE_STORE_MAX_GAMES` to `score_store.h`. Declare the mapping function and this migration helper:

```c
void Score_Store_Migrate_Legacy_Entries(uint8_t legacy_game_count,
    const uint8_t legacy_entry_count[SCORE_STORE_MAX_GAMES],
    const Score_entry legacy_entries[SCORE_STORE_MAX_GAMES][SCORE_STORE_TOP_COUNT],
    uint8_t current_game_count, uint8_t current_entry_count[SCORE_STORE_MAX_GAMES],
    Score_entry current_entries[SCORE_STORE_MAX_GAMES][SCORE_STORE_TOP_COUNT]);
```

In `score_store.c`, set `SCORE_STORE_VERSION` to `4u` and implement the mapping:

```c
uint8_t Score_Store_Map_Legacy_Id(uint8_t legacy_id) {
    static const uint8_t legacy_to_current[] = {
        0u, 1u, 0xffu, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u,
        10u, 11u, 12u, 16u, 17u, 20u, 15u, 18u, 13u, 14u, 19u,
    };
    return legacy_id < sizeof(legacy_to_current) ? legacy_to_current[legacy_id] : 0xffu;
}
```

The helper clears the current arrays, loops through `legacy_game_count`, skips mappings of `0xffu` or values outside `current_game_count`, clamps each count to `SCORE_STORE_TOP_COUNT`, and copies that many `Score_entry` objects with `memcpy`.

Read same-sized version-3 files into a temporary `Score_file`, validate the legacy checksum, reset the version-4 destination, then call the helper. Update version-1 migration to send each old slot through the same mapping before creating its `LEGACY` entry. Mark both migrations dirty so `Score_Store_Commit()` rewrites version 4.

- [ ] **Step 4: Run the unit test and VM build**

```bash
cc -std=c99 -Wall -Wextra -Werror -DFRAMEWORK_USE_LFS=0 \
  -Isrc/app/game_console -Isrc/vm/syscall \
  tests/app/score_store_id_migration_test.c src/app/game_console/score_store.c \
  -o /tmp/score_store_id_migration_test
/tmp/score_store_id_migration_test
python3 scripts/cc.py --target vm
```

Expected: host test exits 0 and VM build succeeds.

- [ ] **Step 5: Commit**

```bash
git add tests/app/score_store_id_migration_test.c src/app/game_console/score_store.h src/app/game_console/score_store.c
git commit -m "feat: migrate score ids to registry order"
```

### Task 2: Generate Contiguous IDs from the Entry List

**Files:**
- Create: `src/app/game_console/game_entries.inc`
- Create: `tests/app/game_entry_registry_test.py`
- Modify: `src/app/game_console/game_registry.h`
- Modify: `src/app/game_console/game_registry.c`

**Interfaces:**
- Consumes: score migration from Task 1.
- Produces: contiguous `Game_id` enum in the exact 21-token menu order.

- [ ] **Step 1: Write the failing entry-list contract test**

Create `tests/app/game_entry_registry_test.py` with:

```python
import pathlib
import re
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]
EXPECTED = [
    "pacman", "snake", "tank_battle", "air_battle", "tetris", "breakout", "pong",
    "gomoku", "game_2048", "dino_runner", "flappy_bird", "maze", "needle",
    "dodge_box", "rhythm", "sfx_lib", "calculator", "fps_test", "volume_control",
    "bad_apple", "info",
]


class GameEntryRegistryTest(unittest.TestCase):
    def test_entry_list_has_canonical_order(self):
        text = (ROOT / "src/app/game_console/game_entries.inc").read_text()
        self.assertEqual(re.findall(r"^game_entry\((\w+)\)", text, re.MULTILINE), EXPECTED)

    def test_ids_are_generated_from_entry_list(self):
        header = (ROOT / "src/app/game_console/game_registry.h").read_text()
        self.assertIn("#define game_entry(name) game_id_##name,", header)
        self.assertNotIn("game_id_racing", header)


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Step 2: Run and observe the missing list**

```bash
python3 -m unittest tests/app/game_entry_registry_test.py -v
```

Expected: error for missing `game_entries.inc` and failure for the handwritten enum.

- [ ] **Step 3: Add the token list and generated enum**

Create `game_entries.inc` with the exact `EXPECTED` order and the motion-style language-server fallback:

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

Replace the handwritten enum with the generated form. Update the still-central registry descriptors to the new names: `game_id_tank_battle`, `game_id_air_battle`, `game_id_game_2048`, `game_id_dino_runner`, `game_id_flappy_bird`, `game_id_dodge_box`, `game_id_sfx_lib`, `game_id_calculator`, `game_id_fps_test`, and `game_id_volume_control`.

- [ ] **Step 4: Verify contracts and build**

```bash
python3 -m unittest tests/app/game_entry_registry_test.py -v
python3 scripts/cc.py --target vm
```

Expected: two tests pass and VM build succeeds.

- [ ] **Step 5: Commit**

```bash
git add src/app/game_console/game_entries.inc src/app/game_console/game_registry.h \
  src/app/game_console/game_registry.c tests/app/game_entry_registry_test.py
git commit -m "refactor: generate game ids from entry list"
```

### Task 3: Add Descriptor Icon Interface and First Entry Batch

**Files:**
- Modify: `src/app/game_console/game_registry.h`
- Modify: `tests/app/game_entry_registry_test.py`
- Modify: primary sources for `pacman`, `snake`, `tank_battle`, `air_battle`, `tetris`, `breakout`, and `pong`.

**Interfaces:**
- Produces: `Game_draw_icon`, descriptor `draw_icon/name_color`, and seven `game_<token>_entry` symbols.

- [ ] **Step 1: Extend the contract test before production code**

Add a helper that searches primary sources for exactly one descriptor definition, then assert the first seven tokens:

```python
def entry_source(token):
    return ROOT / "src/app/games" / token / f"{token}.c"

def assert_entry(self, token):
    text = entry_source(token).read_text()
    self.assertEqual(len(re.findall(rf"const Game_descriptor\s+game_{token}_entry\s*=", text)), 1)
    self.assertIn(f".id = game_id_{token}", text)
    self.assertRegex(text, r"\.draw_icon\s*=\s*\w+")
    self.assertRegex(text, r"\.name_color\s*=\s*[^,]+")
    self.assertRegex(text, r"\.init\s*=\s*\w+")
    self.assertRegex(text, r"\.update\s*=\s*\w+")
    self.assertRegex(text, r"\.get_score\s*=\s*\w+")
    self.assertRegex(text, r"\.is_game\s*=\s*[01]")

def test_first_entry_batch(self):
    for token in EXPECTED[:7]:
        self.assert_entry(token)
```

Expected before implementation: seven assertion failures.

- [ ] **Step 2: Add the common callback fields**

In `game_registry.h`, keep `Game_icon icon` temporarily for the central fallback and add:

```c
typedef void (*Game_draw_icon)(St7789* lcd, int32_t x, int32_t y);

Game_draw_icon draw_icon;
uint16_t name_color;
```

- [ ] **Step 3: Move first-batch metadata and copy icons into their sources**

For each token, add a `static <token>_draw_icon(St7789* lcd, int32_t x, int32_t y)` using the existing console icon body translated into a 48×40 local canvas, then define `game_<token>_entry` with the exact metadata currently in `g_games[]`. Use name colors `0xffe0u` for Pacman, `0x07e0u` for Snake, and `0x07ffu` for the other five. Keep the old exported lifecycle functions and central descriptors until Task 6 so the intermediate build remains unchanged.

- [ ] **Step 4: Run the new test and VM build**

```bash
python3 -m unittest tests/app/game_entry_registry_test.py -v
python3 scripts/cc.py --target vm
```

Expected: first-batch descriptor test passes and VM build succeeds.

- [ ] **Step 5: Commit**

```bash
git add tests/app/game_entry_registry_test.py src/app/game_console/game_registry.h \
  src/app/games/{pacman,snake,tank_battle,air_battle,tetris,breakout,pong}
git commit -m "refactor: colocate first game entries"
```

### Task 4: Colocate the Second Entry Batch

**Files:**
- Modify: `tests/app/game_entry_registry_test.py`
- Modify: primary sources for `gomoku`, `game_2048`, `dino_runner`, `flappy_bird`, `maze`, `needle`, and `dodge_box`.

**Interfaces:**
- Produces: seven more descriptors and icon callbacks using the Task 3 interface.

- [ ] **Step 1: Add and run the second-batch failing assertion**

```python
def test_second_entry_batch(self):
    for token in EXPECTED[7:14]:
        self.assert_entry(token)
```

Run the unittest command; expected: failures for all seven new descriptors.

- [ ] **Step 2: Move second-batch metadata and icons**

Copy each corresponding icon implementation from `game_console.c`, translate it to the 48×40 canvas, and add its descriptor with the existing name/help/control/is-game values. Set every second-batch `name_color` to `0x07ffu`. Keep current public lifecycle functions for Task 6.

- [ ] **Step 3: Verify and commit**

```bash
python3 -m unittest tests/app/game_entry_registry_test.py -v
python3 scripts/cc.py --target vm
git add tests/app/game_entry_registry_test.py \
  src/app/games/{gomoku,game_2048,dino_runner,flappy_bird,maze,needle,dodge_box}
git commit -m "refactor: colocate remaining game entries"
```

Expected: all current tests and VM build pass before commit.

### Task 5: Colocate Tool Entries

**Files:**
- Modify: `tests/app/game_entry_registry_test.py`
- Modify: primary sources for `rhythm`, `sfx_lib`, `calculator`, `fps_test`, `volume_control`, `bad_apple`, and `info`.

**Interfaces:**
- Produces: the final seven descriptors and icon callbacks.

- [ ] **Step 1: Add and run the tool-batch failing assertion**

```python
def test_tool_entry_batch(self):
    for token in EXPECTED[14:]:
        self.assert_entry(token)
```

Run the unittest command; expected: failures for all seven new descriptors.

- [ ] **Step 2: Move tool metadata and icons**

Add canvas-local icon callbacks and exact current descriptor metadata. Rhythm remains `is_game = 1`; SFX, Calculator, FPS Test, Volume, Bad Apple, and Info use `is_game = 0`. All seven use `name_color = 0x07ffu`.

- [ ] **Step 3: Verify and commit**

```bash
python3 -m unittest tests/app/game_entry_registry_test.py -v
python3 scripts/cc.py --target vm
git add tests/app/game_entry_registry_test.py \
  src/app/games/{rhythm,sfx_lib,calculator,fps_test,volume_control,bad_apple,info}
git commit -m "refactor: colocate console tool entries"
```

Expected: all 21 descriptor contracts and VM build pass before commit.

### Task 6: Switch Registry and Console to Entry Callbacks

**Files:**
- Modify: `tests/app/game_entry_registry_test.py`
- Modify: `src/app/game_console/game_registry.h`
- Replace: `src/app/game_console/game_registry.c`
- Modify: `src/app/game_console/game_console.c`

**Interfaces:**
- Consumes: all 21 descriptor symbols.
- Produces: generated descriptor pointer array and fully generic icon rendering.

- [ ] **Step 1: Add failing central-decoupling tests**

```python
def test_registry_array_is_generated(self):
    source = (ROOT / "src/app/game_console/game_registry.c").read_text()
    self.assertIn("#define game_entry(name) &game_##name##_entry,", source)

def test_console_has_no_entry_specific_icons(self):
    source = (ROOT / "src/app/game_console/game_console.c").read_text()
    self.assertNotRegex(source, r"game_icon_|draw_\w+_icon")
    self.assertIn("game->draw_icon(g_lcd, icon_x, icon_y);", source)
```

Expected: both tests fail against the central registry and icon switch.

- [ ] **Step 2: Replace registry with X-macro declarations and array**

Use the exact extern and pointer-array expansions from the design. Return `g_games[index]` rather than `&g_games[index]`. Add a C99-compatible typedef assertion:

```c
typedef char Game_registry_count_matches_ids[
    (sizeof(g_games) / sizeof(g_games[0]) == game_id_count) ? 1 : -1];
```

- [ ] **Step 3: Make menu icon rendering generic**

Remove `Game_icon`, the temporary descriptor icon field, all 21 console icon functions, and the dispatch chain. Define the canvas origin once:

```c
const int32_t icon_x = cx + (CELL_W - 48) / 2;
const int32_t icon_y = cy + 4;
if (game->draw_icon != NULL) { game->draw_icon(g_lcd, icon_x, icon_y); }
```

Draw unselected names with `game->name_color` and selected names with `COLOR_WHITE`.

- [ ] **Step 4: Verify behavior contracts and build**

```bash
python3 -m unittest tests/app/game_entry_registry_test.py tests/app/game_contract_test.py -v
python3 scripts/cc.py --target vm
```

Expected: registry/icon tests pass, lifecycle contracts remain green, and VM build succeeds.

- [ ] **Step 5: Commit**

```bash
git add tests/app/game_entry_registry_test.py src/app/game_console/game_registry.h \
  src/app/game_console/game_registry.c src/app/game_console/game_console.c
git commit -m "refactor: dispatch console entries through descriptors"
```

### Task 7: Remove Public Entry Headers and Finalize Documentation

**Files:**
- Modify: all 21 primary entry `.c` files.
- Delete: the corresponding 21 ordinary `.h` files.
- Preserve: `src/app/games/air_battle/air_assets.h` and `src/app/games/info/info_image_*.h`.
- Modify: `tests/app/game_entry_registry_test.py`
- Modify: `docs/modules.md`
- Modify: `docs/developer_guide.md`
- Modify: `docs/freertos_design.md`

**Interfaces:**
- Produces: one externally visible descriptor per primary source and documented add-entry workflow.

- [ ] **Step 1: Add failing encapsulation tests**

Add these methods to `GameEntryRegistryTest`:

```python
def test_primary_entry_headers_are_removed(self):
    for token in EXPECTED:
        self.assertFalse((ROOT / "src/app/games" / token / f"{token}.h").exists())

def test_only_descriptors_keep_external_linkage(self):
    for token in EXPECTED:
        text = entry_source(token).read_text()
        self.assertNotRegex(text, r"\n(?:void|Game_result|uint32_t)\s+[A-Z]\w+_(?:Init|Update|Get_Score)\s*\(")
        self.assertEqual(text.count(f"const Game_descriptor game_{token}_entry"), 1)

def test_resource_headers_remain(self):
    self.assertTrue((ROOT / "src/app/games/air_battle/air_assets.h").exists())
    self.assertTrue((ROOT / "src/app/games/info/info_image_hitsz.h").exists())
```

Expected before implementation: the ordinary-header and external-linkage tests fail.

- [ ] **Step 2: Remove ordinary headers and localize functions**

Replace each source's self-header include with `game_registry.h`. Keep the existing lifecycle function names to minimize churn, but mark init/update/get-score/icon functions `static`. Delete the 21 ordinary headers. Do not delete `air_assets.h` or Info image headers.

- [ ] **Step 3: Update documentation**

Document the two-step add-entry workflow:

```text
1. Add src/app/games/<token>/<token>.c with static callbacks and game_<token>_entry.
2. Add game_entry(<token>) to game_entries.inc at the desired menu/ID position.
```

Update the descriptor example with `draw_icon` and `name_color`, explain contiguous IDs and score version-4 migration, and remove instructions to create a per-game header or edit `game_registry.c` manually.

- [ ] **Step 4: Format and run full verification**

```bash
source scripts/format.bash
cc -std=c99 -Wall -Wextra -Werror -DFRAMEWORK_USE_LFS=0 \
  -Isrc/app/game_console -Isrc/vm/syscall \
  tests/app/score_store_id_migration_test.c src/app/game_console/score_store.c \
  -o /tmp/score_store_id_migration_test
/tmp/score_store_id_migration_test
python3 -m unittest tests/app/game_entry_registry_test.py tests/app/game_contract_test.py -v
! rg -n 'game_icon_|game_id_racing' src/app
git diff --check
python3 scripts/cc.py --target vm
python3 scripts/cc.py --target arm
```

Expected: host migration test exits 0, all Python tests pass, scans are empty, diff check is clean, and VM/ARM builds succeed without warnings.

- [ ] **Step 5: Commit and inspect branch state**

```bash
git add src/app/game_console src/app/games tests/app docs/modules.md docs/developer_guide.md docs/freertos_design.md
git commit -m "docs: describe self-contained game entries"
git status --short --branch
git log --oneline --decorate --max-count=10
```

Expected: clean `feature/game-entry-registry` branch containing the design, plan, migration, registry, entry, console, and documentation commits.
