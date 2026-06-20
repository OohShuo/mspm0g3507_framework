# Game Info Screen Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a lightweight, attractive registry-driven information page opened with X from the menu.

**Architecture:** Keep one newline-formatted read-only string per descriptor. A stateless renderer consumes one line at a time into a 35-byte stack buffer; the console owns the new state and navigation.

**Tech Stack:** C99, existing ST7789 graphics primitives, Python source-contract tests, VM CMake build.

## Global Constraints

- No heap allocation or persistent text buffer.
- Manual line breaks only; maximum 34 visible characters per line.
- Consecutive newlines render consecutive blank lines.
- Preserve unified top/bottom bars and FPS.

---

### Task 1: Newline text iterator

**Files:** Create `src/app/game_console/game_info_text.h`, `src/app/game_console/game_info_text.c`, `tests/game_info_text_test.c`.

- [ ] Write a failing test for `Game_Info_Text_Next_Line`, including `"ONE\n\nTWO"`.
- [ ] Verify the test fails because the API is absent.
- [ ] Implement a bounded 34-character iterator that preserves empty lines.
- [ ] Run the host C test and commit.

### Task 2: Registry information content

**Files:** Modify `src/app/game_console/game_registry.h`, `src/app/game_console/game_registry.c`; create `tests/game_registry_info_test.py`.

- [ ] Write a failing source-contract test requiring every entry to have DESCRIPTION, GOAL, and CONTROLS paragraphs and lines no longer than 34 characters.
- [ ] Verify RED.
- [ ] Add `info_text` and concise manually wrapped content for all 20 entries.
- [ ] Run registry and iterator tests and commit.

### Task 3: Information screen renderer and console state

**Files:** Modify `src/app/game_console/game_info_screen.c`, `src/app/game_console/game_console.c`.

- [ ] Add source-level failing checks for the new state, X entry, B return, and screensaver redraw.
- [ ] Render the bordered card, cyan headings, body text, top bar, and `B BACK` bottom bar.
- [ ] Add `console_state_game_info`, update/dispatch/FPS/screensaver handling, and menu hints.
- [ ] Run all focused tests and build the VM target.
- [ ] Inspect diff, verify resource constraints, and commit.
