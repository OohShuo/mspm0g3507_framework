# Unified Platform Bootstrap Design

## Goal

Use one `src/main.c` for ARM and VM builds. The common entry point owns the
application startup order, while platform-specific static libraries provide
the same platform API and select the required low-level implementations at
link time.

The root `CMakeLists.txt` must no longer contain separate, large ARM and VM
build graphs. Platform-specific composition belongs under `src/platform` and
`src/vm`.

## Directory Layout

```text
src/platform/
├── CMakeLists.txt
├── platform.h
├── platform_arm.c
└── platform_vm.c

src/vm/
├── CMakeLists.txt
├── display_vm.c
├── input_vm.c
├── haptics_vm.c
├── hal/
├── bsp/
└── freertos/
```

Other existing VM implementation files remain under `src/vm`; the tree above
shows the ownership boundary rather than an exhaustive file list.

`src/vm/main_vm.c` is removed after its lifecycle logic is migrated to
`src/platform/platform_vm.c`.

## Platform API

`src/platform/platform.h` exposes the complete platform lifecycle required by
the common entry point:

```c
int Platform_Init(void);
int Platform_Start(void);
```

`Platform_Init()` returns zero on success and a non-zero value when platform
startup fails. This is primarily needed for SDL initialization failures; the
ARM implementation normally returns zero.

`Platform_Start()` starts platform task execution and owns the platform event
loop. On ARM it normally never returns. On VM it returns zero after the user
closes the simulator and cleanup completes.

## Common Main Flow

`src/main.c` contains no TI DriverLib, SDL, or scheduler-specific calls. Its
startup sequence is:

```c
int main(void) {
    if (Platform_Init() != 0) {
        return 1;
    }

    Syscall_Init();
    Local_Lib_Init();
    Bsp_Init();
    Hal_Init();
    App_Init();

    Hal_Task_Def();
#if TEST_ANY_ENABLE
    Test_Task_Def();
#endif
    App_Task_Def();

    return Platform_Start();
}
```

Both platforms therefore share initialization order, task-definition order,
and failure handling at the entry point.

## ARM Implementation

`platform_arm.c` owns code that is currently ARM-specific in `main.c`:

- normalize a debugger-triggered reset when configured;
- call `SYSCFG_DL_init()`;
- enable the DMA interrupt;
- call `vTaskStartScheduler()` from `Platform_Start()`.

If `vTaskStartScheduler()` unexpectedly returns, `Platform_Start()` returns a
non-zero error value rather than silently continuing.

## VM Implementation

`platform_vm.c` owns code that is currently in `main_vm.c`:

- install process signal handlers;
- initialize SDL, display, input, and haptics;
- start the registered VM tasks;
- poll SDL events;
- update input and haptics;
- render the display;
- release haptics, display, and SDL resources on exit.

Device implementations such as display, input, audio, haptics, VM HAL, VM BSP,
and FreeRTOS compatibility remain in the `vm` static library managed by
`src/vm/CMakeLists.txt`.

## VM Task Start Semantics

The current VM `xTaskCreate()` starts a pthread immediately, while ARM
FreeRTOS only begins task execution after the scheduler starts. This difference
can allow VM tasks to run before all initialization and task definitions are
complete.

The VM compatibility layer will instead:

1. record task descriptors in `xTaskCreate()` without starting pthreads;
2. start all recorded tasks when `Platform_Start()` begins;
3. start newly created tasks immediately after task execution has begun, which
   preserves FreeRTOS dynamic task-creation behavior.

This gives the two platforms the same startup boundary.

## CMake Target Model

`src/vm/CMakeLists.txt` creates the `vm` static library. `main_vm.c` is not a
source of this target.

`src/platform/CMakeLists.txt` creates:

- a platform header interface containing `platform.h`;
- an ARM platform implementation target from `platform_arm.c`;
- a VM platform implementation target from `platform_vm.c`;
- one selected `framework_platform` interface target.

The selected interface has this conceptual link graph:

```text
VM:
framework_platform
└── platform_vm
    └── vm + common libraries

ARM:
framework_platform
└── platform_arm
    └── hal + bsp + syscall + freertos + ti + common libraries
```

HAL, BSP, syscall, FreeRTOS, configuration, and platform headers are published
through dedicated `INTERFACE` targets. Consumers obtain declarations through
target usage requirements instead of manually duplicating include-directory
lists.

The root build retains only the minimal platform setup that must happen before
`project()`, such as selecting the ARM toolchain and enabled languages. It then
adds common subdirectories, builds the executable from `src/main.c`, and links
`framework_platform`.

## Dependency Rules

- `app` depends on platform-neutral HAL/BSP header contracts, not VM headers.
- `platform_vm` may call public VM lifecycle functions but must not expose SDL
  details to `main.c`.
- `vm` must not link back to `app`; the final executable composes `app` and the
  selected platform.
- ARM and VM implementation libraries must not be linked into the same final
  executable because they intentionally provide identical symbols.
- Aggregate interface targets must not create a `vm -> interface -> vm` link
  cycle.

## Error Handling and Shutdown

- VM initialization unwinds already initialized SDL subsystems before
  returning an error.
- ARM initialization reports success after board setup; unrecoverable board
  failures continue to use the project's assertion policy.
- VM shutdown stops the platform loop before deinitializing display and
  haptics resources.
- The common `main.c` maps any `Platform_Init()` failure to process exit code
  `1`.

## Verification

The migration is complete when:

- ARM and VM both compile the same `src/main.c`;
- `src/vm/main_vm.c` no longer exists;
- the ARM build links only ARM platform implementations;
- the VM build links only VM platform implementations;
- VM tasks do not execute before `Platform_Start()`;
- existing ARM and VM builds complete without new warnings;
- the VM opens, accepts input, renders, produces audio/haptic feedback, and
  exits cleanly;
- the ARM binary retains the existing initialization and scheduler behavior.

## Non-Goals

- Refactoring application, game, HAL, or BSP behavior unrelated to platform
  selection;
- replacing the FreeRTOS-compatible VM API;
- adding runtime platform selection;
- supporting multiple platform implementations in one executable.
