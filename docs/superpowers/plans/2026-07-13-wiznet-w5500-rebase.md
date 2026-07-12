# Wiznet/W5500 Rebase Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Rebuild `feat-wiznet` on current `main` so its only functional additions are the complete Wiznet ioLibrary, W5500 HAL, UDP wrapper, and a current-structure hardware test.

**Architecture:** Preserve the complete vendor tree under `lib/wiznet`, conditionally compiling it for `_WIZCHIP_=W5500`. Adapt W5500 and UDP to the current BSP/HAL interfaces and expose a hardware echo test through the existing test runtime.

**Tech Stack:** C99, CMake 3.15+, YAML build driver, FreeRTOS, TI MSPM0 DriverLib, Wiznet ioLibrary

## Global Constraints

- Base all work on current `main`; do not carry old branch structure or unrelated features.
- Keep the complete old `lib/wiznet/` tree.
- Above BSP, add only W5500, UDP, and their test.
- Do not add `docs/wiznet_w5500.md`.
- Keep `FRAMEWORK_USE_WIZNET=OFF` by default for VM and ARM.
- Preserve a backup ref for the old `feat-wiznet` head before replacing it.

---

### Task 1: Preserve History and Import the Vendor Library

**Files:**
- Create: `lib/wiznet/**`
- Create: `cmake/wiznet_config.cmake`
- Modify: `lib/CMakeLists.txt`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: old `feat-wiznet`, current `main`
- Produces: CMake target `wiznet` and public `socket.h`, `wizchip_conf.h`, W5500 headers

- [ ] **Step 1: Preserve the old branch head**

```bash
git branch backup/feat-wiznet-pre-rebase feat-wiznet
git rev-parse backup/feat-wiznet-pre-rebase
```

Expected: the second command prints the original head `f9fe685...`.

- [ ] **Step 2: Observe the missing feature integration**

Temporarily pass `-DFRAMEWORK_USE_WIZNET=ON` to a CMake configure before registering the option.

```bash
cmake -S . -B /tmp/framework-wiznet-red -G Ninja -DBUILD_PLATFORM=VM -DFRAMEWORK_USE_WIZNET=ON
cmake --build /tmp/framework-wiznet-red --target wiznet
```

Expected: FAIL because target `wiznet` does not exist.

- [ ] **Step 3: Import and configure the complete library**

```bash
git restore --source=backup/feat-wiznet-pre-rebase -- lib/wiznet
```

Create `cmake/wiznet_config.cmake`:

```cmake
set(WIZNET_W5500_ENABLED ON)
```

Append `FRAMEWORK_USE_WIZNET` to `_framework_use_keys` in top-level `CMakeLists.txt`. Add this before `target_link_libraries` in `lib/CMakeLists.txt`:

```cmake
if(FRAMEWORK_USE_WIZNET)
    include("${CMAKE_SOURCE_DIR}/cmake/wiznet_config.cmake")
    add_subdirectory(wiznet)
    list(APPEND _framework_lib_deps wiznet)
endif()
```

Retain the imported library CMake selection logic and add:

```cmake
target_compile_definitions(wiznet PUBLIC _WIZCHIP_=W5500)
```

- [ ] **Step 4: Verify the target and vendor tree**

```bash
cmake -S . -B /tmp/framework-wiznet-green -G Ninja -DBUILD_PLATFORM=VM -DFRAMEWORK_USE_WIZNET=ON
cmake --build /tmp/framework-wiznet-green --target wiznet
git diff --no-index <(git ls-tree -r --name-only backup/feat-wiznet-pre-rebase lib/wiznet) <(git ls-files lib/wiznet)
```

Expected: the target builds; the path comparison shows no missing vendor files.

- [ ] **Step 5: Commit**

```bash
git add CMakeLists.txt cmake/wiznet_config.cmake lib/CMakeLists.txt lib/wiznet
git commit -m "feat: import complete wiznet ioLibrary"
```

### Task 2: Add W5500 and UDP HAL Integration

**Files:**
- Create: `src/hal/w5500/w5500_hal.h`
- Create: `src/hal/w5500/w5500_hal.c`
- Create: `src/hal/com_udp/com_udp.h`
- Create: `src/hal/com_udp/com_udp.c`
- Modify: `src/hal/hal.c`

**Interfaces:**
- Consumes: `Bsp_Hard_Spi_Read_Blocking`, `Bsp_Hard_Spi_Write_Blocking`, `Bsp_Gpio_Write`, Wiznet socket APIs
- Produces: `W5500_Create`, `W5500_Reset`, `Com_Udp_Init`, `Com_Udp_Create`, `Com_Udp_Poll`, `Com_Udp_Send`, `Com_Udp_Get_Src`

- [ ] **Step 1: Write a compile-first failing consumer**

Create `src/test/w5500_udp/test_w5500_udp.c`:

```c
#include "com_udp.h"
#include "w5500_hal.h"
void Test_W5500_Udp_Task_Def(void) {
    W5500_Config cfg = {0};
    (void)W5500_Create(&cfg);
    Com_Udp_Init();
}
```

Build ARM with `FRAMEWORK_USE_WIZNET=ON`. Expected: FAIL because the HAL headers do not exist.

- [ ] **Step 2: Port the W5500 interface and implementation**

Define in `w5500_hal.h`:

```c
typedef struct {
    uint32_t spi_idx, cs_gpio_idx, rst_gpio_idx;
    SemaphoreHandle_t spi_mutex;
} W5500_Config;
typedef struct { W5500_Config config; uint8_t inited; } Wiz5500;
Wiz5500* W5500_Create(const W5500_Config* cfg);
void W5500_Reset(Wiz5500* obj);
SemaphoreHandle_t W5500_Get_Mutex(Wiz5500* obj);
```

Port the singleton implementation from the backup branch. Register CS, byte SPI, burst SPI, and critical-section callbacks using the current BSP functions. Reject null configuration and duplicate creation.

- [ ] **Step 3: Port the UDP interface and implementation**

Retain the old `Com_udp_config` fields for W5500, MAC/IP/subnet/gateway, socket number, port, buffers, and receive callback. Add these validations:

```c
if (cfg == NULL || g_instances == NULL || cfg->wiz == NULL) return NULL;
if (cfg->sock_n >= 8 || cfg->rx_buf_size == 0 || cfg->tx_buf_size == 0) return NULL;
```

Retain one-time `wizchip_init`, static `ctlnetwork`, UDP socket reopen, bounded send, polling callback, and source-address retrieval.

- [ ] **Step 4: Integrate initialization**

In `src/hal/hal.c`, conditionally include `com_udp.h` and call:

```c
#if FRAMEWORK_USE_WIZNET
    Com_Udp_Init();
#endif
```

at the end of `Hal_Init()`.

- [ ] **Step 5: Build the consumer again**

Expected: missing-header and missing-symbol failures disappear. Record toolchain or board-configuration failures separately.

- [ ] **Step 6: Commit**

```bash
git add src/hal/w5500 src/hal/com_udp src/hal/hal.c
git commit -m "feat: integrate w5500 udp hal"
```

### Task 3: Add Current Configuration and Hardware Test

**Files:**
- Modify: `config/config.yaml`
- Modify: `config/test_config.h`
- Modify: `src/test/test.c`
- Create: `src/test/w5500_udp/test_w5500_udp.h`
- Modify: `src/test/w5500_udp/test_w5500_udp.c`

**Interfaces:**
- Consumes: Task 2 APIs and current test runtime
- Produces: `Test_W5500_Udp_Task_Def()` selected by `TEST_W5500_UDP_ENABLE`

- [ ] **Step 1: Verify the dependency guard fails**

Add to `test_config.h`, include it in `TEST_ANY_ENABLE`, and temporarily set it to `1`:

```c
#define TEST_W5500_UDP_ENABLE 1
#if TEST_W5500_UDP_ENABLE && !FRAMEWORK_USE_WIZNET
    #error "TEST_W5500_UDP_ENABLE requires FRAMEWORK_USE_WIZNET=ON"
#endif
```

Run default ARM preprocessing. Expected: FAIL with the exact dependency error. Restore its default to `0`.

- [ ] **Step 2: Register the test**

Create `test_w5500_udp.h` declaring `Test_W5500_Udp_Task_Def`. Include it in `src/test/test.c` and call it under `#if TEST_W5500_UDP_ENABLE`.

- [ ] **Step 3: Implement the hardware echo test**

Create one W5500 and one UDP socket using constants grouped at the top of the test file. The receive callback must echo to its source:

```c
static void on_rx(Com_udp* obj, const uint8_t* data, uint32_t len, uint8_t flags, void* arg) {
    (void)flags; (void)arg;
    uint8_t ip[4]; uint16_t port;
    Com_Udp_Get_Src(obj, ip, &port);
    Com_Udp_Send(obj, data, len, ip, port);
}
```

The FreeRTOS task calls `Com_Udp_Poll()` every 10 ms. Use explicit adjustable constants for SPI index, CS/RESET GPIO indices, MAC, static IP, subnet, gateway, socket 0, port, and bounded buffers.

- [ ] **Step 4: Add YAML options**

Add `FRAMEWORK_USE_WIZNET: OFF` to both VM and ARM targets. Do not enable the hardware test by default.

- [ ] **Step 5: Verify configurations**

```bash
python3 scripts/cc.py --target vm
python3 scripts/cc.py --target arm
```

Expected: VM does not link Wiznet; ARM builds when local toolchain/SysConfig inputs are available. Also temporarily enable both options for an ARM compile/link check.

- [ ] **Step 6: Commit**

```bash
git add config/config.yaml config/test_config.h src/test/test.c src/test/w5500_udp
git commit -m "test: add w5500 udp hardware test"
```

### Task 4: Replace the Old Branch and Audit the Result

**Files:**
- Verify all paths changed from `main`

**Interfaces:**
- Consumes: verified `feat-wiznet-rebased`
- Produces: rebuilt `feat-wiznet` plus `backup/feat-wiznet-pre-rebase`

- [ ] **Step 1: Run fresh verification**

```bash
python3 scripts/cc.py --target vm
python3 scripts/cc.py --target arm
git diff --check main...HEAD
git merge-base --is-ancestor main HEAD
```

Expected: available builds and ancestry check exit 0; diff check is silent.

- [ ] **Step 2: Audit changed paths**

```bash
git diff --name-status main...HEAD
git diff --stat main...HEAD
```

Allowed paths: top-level/build CMake integration, both config files, design/plan records, `lib/wiznet/**`, W5500/UDP HAL, `hal.c`, test registry, and `src/test/w5500_udp/**`. `docs/wiznet_w5500.md` and all unrelated old paths must be absent.

- [ ] **Step 3: Move the requested branch name**

```bash
git branch -f feat-wiznet HEAD
git switch feat-wiznet
git branch -D feat-wiznet-rebased
```

Expected: `feat-wiznet` points at the rebuilt head and the backup ref remains at the original history.

- [ ] **Step 4: Report rewrite implications**

Do not push. Report the new head, backup ref, verification evidence, and that updating the remote will require a separately approved force-with-lease push.
