# Architecture Decision Records

## 1. Layered Architecture

**Decision:** APP → HAL → BSP → DriverLib, four-layer one-way dependency.

**Reason:** Two layers (APP+DriverLib) cannot support VM; three layers (merge HAL+BSP) couples pin mapping to driver logic, requiring rewriting all HAL when changing MCU; five layers adds unnecessary indirection overhead.

**Tradeoff:** Each new peripheral requires editing 4 files (APP/HAL/BSP/config); init ordering is manual; 3 layers of indirection (effectively zero overhead under `-Os` due to inlining — verify via map file).

**Alternatives rejected:** Flat structure, HAL-only, 3-layer, Zephyr devicetree.

---

## 2. FreeRTOS

**Decision:** FreeRTOS v11.x as RTOS kernel.

**Reason:** Mature Cortex-M0+ port (handles lack of CLZ instruction), ~12KB Flash (current build), heap_4 best-fit prevents fragmentation, widely documented.

**Tradeoff:** No built-in device model / shell / filesystem; Timer task at highest priority may preempt application tasks; stack overflow detection is limited on M0+ (no MPU).

**Alternatives rejected:** RTX (license, coupling), CMSIS-RTOS2 (API wrapper, not kernel), Zephyr (too large), bare-metal (reinvent scheduler), RT-Thread (~30KB kernel).

---

## 3. LittleFS

**Decision:** LittleFS as embedded filesystem.

**Reason:** Copy-on-write → survives power loss. Dynamic wear leveling → no fixed superblock. Static RAM (528B) → does not grow with file count.

**Tradeoff:** Scans 2 MiB region on mount (once at startup); COW write amplification (acceptable for infrequent score writes); single-threaded (protected by FreeRTOS mutex).

**Alternatives rejected:** FatFS (no wear leveling, FAT corruption), SPIFFS (unmaintained, O(n) directory), raw binary (manual wear leveling).

---

## 4. LVGL

**Decision:** LVGL v9.5 integrated but disabled by default (`FRAMEWORK_USE_LVGL=OFF`).

**Reason:** Games use direct framebuffer rendering (effectively 0 RAM overhead in the current design). LVGL suits widget UI (settings, file manager) but is over-abstraction for games. Optional compilation avoids forcing Flash/RAM cost.

**Tradeoff:** Maintaining two rendering paths (LVGL + Game_Graphics) increases complexity.

**Alternatives rejected:** uGUI (too minimal), emWin (license + closed source), custom widget toolkit (duplicate work), always-on LVGL (wastes ~45KB Flash + ~30KB RAM under current config).

---

## 5. SDL2 VM Simulator

**Decision:** Build SDL2 VM rather than QEMU/Renode/dual codebase.

**Reason:** QEMU has no MSPM0G3507 model. Renode requires writing C# peripheral models. VM only needs stubs for HAL/BSP. Under current config, typical iteration cycle: hardware ~42s vs VM ~19s, roughly 3× iteration density (measured; actual times depend on binary size and host specs).

**Tradeoff:** Each new HAL module needs a VM stub. Does not simulate SPI timing / ADC noise / Flash wear. No hard real-time guarantees.

**Alternatives rejected:** QEMU, Renode, hardware-only, separate PC engine.

---

## 6. Dual-Region Storage

**Decision:** W25Q32 split into Raw Flash (2 MiB) + LittleFS (2 MiB).

**Reason:** Game assets (large, read-only blocks) don't need filesystem overhead; score persistence needs wear leveling and atomic updates. Partition isolation: formatting the filesystem won't erase assets.

**Tradeoff:** Fixed boundary (requires reformat if changed); two code paths.

**Alternatives rejected:** All-LittleFS (metadata overhead for large assets), all Raw Flash (manual wear leveling).

---

## 7. Game Console Architecture

**Decision:** Game descriptor pattern + 3-state state machine.

**Reason:** Function-pointer struct compiles to effectively zero overhead (equivalent to switch). Adding a game only adds a descriptor, no changes to console core. Static registry; linker automatically garbage-collects unused game code.

**Tradeoff:** Descriptor boilerplate (6 fields). Games cannot communicate directly. Fixed interface limits special I/O needs.

**Known debts:** Icons hardcoded in `game_console.c` (should move to descriptor); `g_games[]` static ordering (no runtime sort).

**Alternatives rejected:** C++ virtual methods (vtable + RTTI overhead), plugin system (requires ELF loader), single-loop without state machine, event-driven updates.
