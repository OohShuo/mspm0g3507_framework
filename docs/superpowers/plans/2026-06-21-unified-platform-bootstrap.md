# 统一平台启动架构实施计划

> **面向执行代理：** 必须使用 `subagent-driven-development`（推荐）或 `executing-plans` 按任务实施，并使用复选框跟踪进度。

**目标：** ARM 与 VM 使用同一个 `src/main.c`，通过链接名为 `platform` 的接口目标选择不同平台实现。

**架构：** `src/platform` 提供统一生命周期接口和两个平台实现；`src/vm` 作为静态库提供 VM 设备及兼容层。公共入口负责初始化和任务定义顺序，平台库负责芯片或 SDL 初始化以及启动调度器或事件循环。

**技术栈：** C99、CMake 3.15、FreeRTOS、pthread、SDL2、TI MSPM0 DriverLib、Python `unittest`。

## 全局约束

- 公共接口目标命名为 `platform`，不得使用 `framework_platform`。
- ARM 和 VM 编译同一个 `src/main.c`，迁移后删除 `src/vm/main_vm.c`。
- `main.c` 不包含 TI DriverLib、SDL 或 `task.h`。
- VM 初始任务不在 `Platform_Start()` 前执行，启动后仍支持动态创建任务。
- `vm` 不反向链接 `app`，不形成 `vm -> interface -> vm` 链接环。
- 不修改与平台启动无关的游戏、HAL 或 BSP 行为。

---

### 任务 1：统一 VM 任务启动边界

**文件：**
- 新建：`src/vm/freertos/task_vm.h`
- 新建：`tests/vm_freertos_start_test.c`
- 修改：`src/vm/freertos/freertos_stubs.c`

**接口：**
- 输入：现有 `xTaskCreate(...)`。
- 输出：`BaseType_t Vm_Freertos_Start_Tasks(void)`。

- [ ] **步骤 1：编写失败测试**

测试创建一个启动前任务，等待 5 ms 后断言它尚未执行；调用 `Vm_Freertos_Start_Tasks()` 后等待并断言执行一次；再动态创建第二个任务并断言它立即执行。计数器使用 `volatile uint32_t`，等待循环最多执行 100 次 `SDL_Delay(1)`。

- [ ] **步骤 2：确认测试失败**

```bash
cc -std=c99 -Wall -Wextra -Werror $(sdl2-config --cflags) \
  -Isrc/vm/freertos tests/vm_freertos_start_test.c \
  src/vm/freertos/freertos_stubs.c $(sdl2-config --libs) -pthread \
  -o /tmp/vm_freertos_start_test
```

预期：缺少 `task_vm.h` 或 `Vm_Freertos_Start_Tasks`，编译失败。

- [ ] **步骤 3：实现私有启动接口**

`task_vm.h` 的完整接口：

```c
#pragma once
#include "FreeRTOS.h"
BaseType_t Vm_Freertos_Start_Tasks(void);
```

在 `freertos_stubs.c` 中增加 `g_started` 和 pthread 互斥锁，将线程创建提取为 `start_task()`。`xTaskCreate()` 在启动前只登记描述符，启动后立即启动线程；`Vm_Freertos_Start_Tasks()` 在锁内遍历并启动所有已登记任务，重复调用不重复启动。

- [ ] **步骤 4：确认测试通过并提交**

重复步骤 2 的编译命令并执行 `/tmp/vm_freertos_start_test`，预期退出码为 0。

```bash
git add src/vm/freertos/freertos_stubs.c src/vm/freertos/task_vm.h tests/vm_freertos_start_test.c
git commit -m "refactor: align VM task startup semantics"
```

---

### 任务 2：建立 Platform 接口并统一 main

**文件：**
- 新建：`src/platform/platform.h`
- 新建：`src/platform/platform_arm.c`
- 新建：`src/platform/platform_vm.c`
- 新建：`src/platform/CMakeLists.txt`
- 新建：`src/vm/syscall/retarget_vm.c`
- 新建：`tests/platform_bootstrap_test.py`
- 修改：`CMakeLists.txt`
- 修改：`src/main.c`
- 删除：`src/vm/main_vm.c`

**接口：**
- 输入：`Vm_Freertos_Start_Tasks()`、VM 设备生命周期、TI SysConfig、FreeRTOS 调度器。
- 输出：`int Platform_Init(void)`、`int Platform_Start(void)`。

- [ ] **步骤 1：编写失败测试**

`tests/platform_bootstrap_test.py` 必须断言：`main.c` 包含 `platform.h`、`Platform_Init()`、`Platform_Start()`；不包含 `SDL`、`SYSCFG_DL_init`、`vTaskStartScheduler`、`task.h`；两个平台实现文件存在；`main_vm.c` 不存在。

运行：

```bash
python3 -m unittest tests.platform_bootstrap_test -v
```

预期：平台文件不存在且旧入口仍存在，测试失败。

- [ ] **步骤 2：创建接口和 ARM 实现**

`platform.h`：

```c
#pragma once
int Platform_Init(void);
int Platform_Start(void);
```

`platform_arm.c` 接管 `normalize_debug_reset()`、`SYSCFG_DL_init()` 和 DMA 中断使能；`Platform_Start()` 调用 `vTaskStartScheduler()`，意外返回时返回 `-1`。

- [ ] **步骤 3：迁移 VM 生命周期**

`platform_vm.c` 接管信号处理、SDL/显示/输入/振动初始化、事件循环和逆序清理。`Platform_Start()` 必须先检查 `Vm_Freertos_Start_Tasks() == pdPASS`，然后轮询事件、更新输入及振动、渲染并延时 5 ms。

- [ ] **步骤 4：统一入口并删除旧入口**

`main.c` 按顺序调用：`Platform_Init()`；Syscall/Local_Lib/Bsp/Hal/App 初始化；Hal/Test/App 任务定义；最后返回 `Platform_Start()`。删除 `src/vm/main_vm.c`。在 `retarget_vm.c` 中提供空的 `Syscall_Init()`，使公共入口在 VM 下保持相同调用顺序。

- [ ] **步骤 5：最小接入现有 CMake**

`src/platform/CMakeLists.txt` 先按 `BUILD_PLATFORM` 选择编译 `platform_arm.c` 或 `platform_vm.c`，并创建 `platform INTERFACE`。现有根 CMake 的两个分支都添加该子目录、改用 `src/main.c` 作为入口，并把 `platform` 链接到最终可执行文件。这一步不做根构建的大规模整理，只保证本任务提交在两个平台上均可构建。

- [ ] **步骤 6：确认测试和构建通过并提交**

```bash
python3 -m unittest tests.platform_bootstrap_test -v
python3 scripts/cc.py --target vm
python3 scripts/cc.py --target arm
git add CMakeLists.txt src/main.c src/platform src/vm/syscall/retarget_vm.c tests/platform_bootstrap_test.py
git rm src/vm/main_vm.c
git commit -m "refactor: unify platform bootstrap entry"
```

预期：所有 `PlatformBootstrapTest` 测试通过，两个平台均完成链接。

---

### 任务 3：重组 CMake 平台目标

**文件：**
- 修改：`src/platform/CMakeLists.txt`
- 新建：`tests/platform_cmake_test.py`
- 修改：`src/vm/CMakeLists.txt`
- 修改：`src/app/CMakeLists.txt`
- 修改：`lib/CMakeLists.txt`
- 修改：`CMakeLists.txt`

**接口：**
- 输入：两个平台实现、`vm`、ARM HAL/BSP/syscall/FreeRTOS/TI。
- 输出：统一 `platform` 接口目标和公共入口可执行文件。

- [ ] **步骤 1：编写失败测试**

`tests/platform_cmake_test.py` 必须断言：平台 CMake 包含 `add_library(platform INTERFACE)` 且不含 `framework_platform`；VM CMake 创建 `vm STATIC` 且不引用 `main_vm.c`；根 CMake 添加 `src/platform` 并调用 `platform_add_executable`，且不再定义 `ENTRANCE_MAIN_C`。

运行 `python3 -m unittest tests.platform_cmake_test -v`，预期失败。

- [ ] **步骤 2：拆分 VM 静态库**

`src/vm/CMakeLists.txt` 从 `VM_SRC` 排除 `freertos/freertos_stubs.c`，单独创建 VM `freertos STATIC`；创建 `vm STATIC` 并链接 `freertos`、`lib`、SDL2。保留 VM 影子头文件优先级，但公共 HAL/BSP/syscall 头文件改由平台头文件接口传播。

- [ ] **步骤 3：创建平台组合层**

`src/platform/CMakeLists.txt` 创建 `platform_headers INTERFACE` 和 `platform INTERFACE`。VM 配置创建 `platform_vm STATIC` 并让 `platform` 传递 `platform_vm` 与 `vm`；ARM 配置创建 `platform_arm STATIC` 并让 `platform` 传递 `platform_arm`、`hal`、`bsp`、`syscall`、`test`。

平台模块负责添加各平台所需子目录，并定义 `platform_add_executable()`：VM 分支创建 `framework_vm`，ARM 分支创建 `framework.elf`；两个分支都将调用方传入的 `src/main.c` 和资源文件加入目标。ARM 分支同时迁入现有 CPU 参数、链接脚本、SysConfig 依赖、hex/bin 后处理和 size 输出，VM 分支迁入 SDL2 链接设置。

- [ ] **步骤 4：精简根构建和依赖**

根构建主体改为：

```cmake
add_subdirectory(src/platform)
add_subdirectory(lib)
add_subdirectory(src/app)
file(GLOB ASSETS_C "${ASSETS_DIR}/*.c")
platform_add_executable("${SRC_DIR}/main.c" ${ASSETS_C})
```

仅在 `project()` 前保留 ARM 工具链和语言选择所需的小型判断。`app` 链接 `platform lib`；`lib` 使用平台模块已经创建的 `freertos`，不再创建 VM 假目标。

- [ ] **步骤 5：确认测试和双平台构建通过**

```bash
python3 -m unittest tests.platform_cmake_test -v
python3 scripts/cc.py --target vm
python3 scripts/cc.py --target arm
rg -n "platform_(arm|vm)" build/vm/build.ninja build/arm/build.ninja
```

预期：结构测试通过；VM 只包含 `platform_vm`；ARM 只包含 `platform_arm`。

- [ ] **步骤 6：提交 CMake 重组**

```bash
git add CMakeLists.txt lib/CMakeLists.txt src/app/CMakeLists.txt \
  src/platform/CMakeLists.txt src/vm/CMakeLists.txt tests/platform_cmake_test.py
git commit -m "refactor: select platform implementations by target"
```

---

### 任务 4：全量验证

**文件：** 无预期源码修改。

**接口：** 输入为前三项任务的实现；输出为验证通过的 ARM/VM 构建。

- [ ] **步骤 1：运行测试**

```bash
python3 -m unittest discover -s tests -p '*_test.py' -v
/tmp/vm_freertos_start_test
```

预期：全部通过，无失败或错误。

- [ ] **步骤 2：从空目录构建**

```bash
rm -rf build/vm build/arm
python3 scripts/cc.py --target vm
python3 scripts/cc.py --target arm
```

预期：两个平台均完成配置、编译和链接，无新增警告。

- [ ] **步骤 3：运行 VM 冒烟测试**

```bash
SDL_VIDEODRIVER=dummy timeout 3s ./build/vm/framework_vm
```

预期：进程持续运行到超时，没有初始化崩溃。

- [ ] **步骤 4：检查唯一入口和差异**

```bash
rg -n "int main\(" src --glob '*.c'
git diff --check
git status --short
```

预期：只有 `src/main.c` 定义 `main()`，差异检查无错误，工作区没有遗漏修改。
