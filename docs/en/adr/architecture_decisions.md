# Architecture Decision Records

## 1. Layered Architecture

**Decision:** APP → HAL → BSP → DriverLib 四层单向依赖。

**Reason:** 两层（APP+DriverLib）无法实现 VM；三层（合并 HAL+BSP）导致引脚映射与驱动耦合，换 MCU 需重写所有 HAL；五层增加不必要的间接调用开销。

**Tradeoff:** 每个新外设需编辑 4 个文件（APP/HAL/BSP/config）；初始化顺序手动维护；3 层间接调用（`-Os` 下内联为零开销）。

**Alternatives rejected:** Flat structure, HAL-only, 3-layer, Zephyr devicetree.

---

## 2. FreeRTOS

**Decision:** FreeRTOS v11.x 作为 RTOS 内核。

**Reason:** Cortex-M0+ 成熟移植（处理无 CLZ 指令），~12KB Flash，heap_4 best-fit 防止碎片化，广泛文档。

**Tradeoff:** 无内置设备模型 / shell / 文件系统；Timer task 最高优先级可能抢断应用任务；栈溢出检测在 M0+ 上有限（无 MPU）。

**Alternatives rejected:** RTX (license, coupling), CMSIS-RTOS2 (API wrapper, not kernel), Zephyr (too large), bare-metal (reinvent scheduler), RT-Thread (~30KB kernel).

---

## 3. LittleFS

**Decision:** LittleFS 作为嵌入式文件系统。

**Reason:** Copy-on-write → 掉电不损坏。Dynamic wear leveling → 无固定超块。Static RAM (528B) → 不随文件数增长。

**Tradeoff:** 挂载时扫描 2 MiB 区域（仅启动一次）；COW 写放大（游戏分数写入不频繁可接受）；单线程（已加 FreeRTOS mutex）。

**Alternatives rejected:** FatFS (no wear leveling, FAT corruption), SPIFFS (unmaintained, O(n) directory), raw binary (manual wear leveling).

---

## 4. LVGL

**Decision:** LVGL v9.5 集成但默认禁用（`FRAMEWORK_USE_LVGL=OFF`）。

**Reason:** 游戏用直接 framebuffer 渲染（0 RAM 开销）。LVGL 适用于 widget UI（设置、文件管理器），但对游戏是过度抽象。可选编译避免强制 Flash/RAM 成本。

**Tradeoff:** 维护两套渲染路径（LVGL + Game_Graphics）增加复杂度。

**Alternatives rejected:** uGUI (too minimal), emWin (license + closed source), custom widget toolkit (duplicate work), always-on LVGL (waste 45KB Flash + 30KB RAM).

---

## 5. SDL2 VM Simulator

**Decision:** 构建 SDL2 VM 而非 QEMU/Renode/双代码库。

**Reason:** QEMU 无 MSPM0G3507 模型。Renode 需写 C# 外设模型。VM 只需为 HAL/BSP 写桩。迭代周期：硬件 42s vs VM 19s，约 3× 迭代密度。

**Tradeoff:** 每个新 HAL 模块需维护 VM 桩。不模拟 SPI 时序 / ADC 噪声 / Flash 磨损。不提供硬实时保证。

**Alternatives rejected:** QEMU, Renode, hardware-only, separate PC engine.

---

## 6. Dual-Region Storage

**Decision:** W25Q32 分成 Raw Flash (2 MiB) + LittleFS (2 MiB)。

**Reason:** 游戏素材（大块只读）不需要文件系统开销；高分持久化需要磨损均衡和原子更新。分区隔离：格式化文件系统不擦除素材。

**Tradeoff:** 固定边界（更改需重新格式化）；两套代码路径。

**Alternatives rejected:** All LittleFS (metadata overhead for large assets), all Raw Flash (manual wear leveling).

---

## 7. Game Console Architecture

**Decision:** Game descriptor 模式 + 3 状态状态机。

**Reason:** Function-pointer struct 编译为零开销（与 switch 相同）。新增游戏只加 descriptor，不改 console 核心。静态注册表，linker 自动垃圾回收未用游戏代码。

**Tradeoff:** Descriptor 样板代码（6 字段）。游戏间不直接通信。固定接口限制特殊 I/O 需求。

**Known debts:** Icons 硬编码在 `game_console.c`（应移到 descriptor）；`g_games[]` 静态顺序（无运行时排序）。

**Alternatives rejected:** C++ virtual methods (vtable + RTTI overhead), plugin system (requires ELF loader), single-loop without state machine, event-driven updates.
