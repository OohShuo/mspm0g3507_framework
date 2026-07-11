# Runtime Mode Configuration Design

## Goal

Make `config/config.yaml` the single selector for the firmware's top-level runtime role while preserving `config/test_config.h` as the selector for individual test tasks.

Each build target accepts:

```yaml
runtime_mode: game # game | flash_mgr | test
```

When the field is absent, the build uses `game` for backward compatibility.

## Mode behavior

| Mode | Game console | Flash manager | Test task dispatcher |
| --- | --- | --- | --- |
| `game` | enabled | disabled | disabled |
| `flash_mgr` | disabled | enabled | disabled |
| `test` | disabled | disabled | enabled when `TEST_ANY_ENABLE` is true |

In `test` mode, `config/test_config.h` remains unchanged. Developers continue to enable one or more `TEST_*_ENABLE` macros there. Selecting `test` with no enabled test is valid and starts no test task.

The modes are mutually exclusive. A build cannot start top-level game, Flash manager, and test workloads together.

## Configuration flow

1. `scripts/cc.py` reads `runtime_mode` from each YAML build target.
2. It normalizes and validates the value against `game`, `flash_mgr`, and `test`.
3. It passes the selected value to CMake as a string cache argument.
4. CMake exposes exactly one compile-time mode definition to framework targets.
5. Application and test startup code derives task creation from that definition.

`runtime_mode` is metadata, not an ON/OFF feature flag, so `cc.py` must not process it through its boolean conversion path.

## Source-level configuration

The build defines three numeric mode constants and one selected mode value, allowing ordinary preprocessor comparisons. The exact names will use a single `FRAMEWORK_RUNTIME_MODE` namespace to avoid collisions.

`config/app_config.h` derives `GAME_CONSOLE_ENABLE` and `FLASH_MGR_ENABLE` from the selected mode instead of containing independently editable values. `config/test_config.h` continues to define all individual test switches and `TEST_ANY_ENABLE`.

`src/main.c` calls `Test_Task_Def()` only when the selected runtime mode is `test` and at least one individual test is enabled. `src/app/app.c` retains its existing task-definition structure, but its application enables are now derived rather than manually configured.

## Validation and errors

An absent `runtime_mode` resolves to `game`. Any present value outside the three supported lowercase names terminates `cc.py` before CMake runs and reports the target name, invalid value, and allowed values.

`flash_mgr` additionally requires both `FRAMEWORK_USE_LFS` and `FRAMEWORK_USE_UART` to be truthy. Configuration headers enforce runtime and test dependencies with compile-time `#error` checks; `cc.py` only validates and forwards the runtime-mode value.

The same mode field is supported for ARM and VM targets so their configuration model stays uniform.

## Tests

Automated Python tests for `scripts/cc.py` will cover:

- defaulting an absent field to `game`;
- mapping each of the three valid values to the correct CMake argument;
- rejecting an invalid value before invoking CMake;
- rejecting incompatible application and test dependencies during preprocessing;
- confirming mode metadata is not converted to an ON/OFF argument.

Build verification will configure at least the `game` and `test` ARM variants. The Flash manager variant will also be configured with both dependencies enabled. Existing project formatting and relevant tests will run after implementation.

## Documentation impact

`config/config.yaml` will explicitly set `runtime_mode` for the existing targets. User-facing build and Flash manager documentation will describe the new selector and remove instructions to edit `GAME_CONSOLE_ENABLE` or `FLASH_MGR_ENABLE` manually.

## Non-goals

- Moving individual `TEST_*_ENABLE` switches into YAML.
- Selecting a particular test automatically.
- Supporting simultaneous top-level runtime roles.
- Changing the Flash UART protocol or game console behavior.
