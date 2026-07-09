# Framework Developer Tool

`scripts/framework.py` provides developer-facing diagnostics for build targets,
tool availability, Flash Manager switches, and linker map summaries.

## Commands

```bash
python3 scripts/framework.py doctor
python3 scripts/framework.py inspect
python3 scripts/framework.py size build/arm/framework.map
```

## Doctor

`doctor` checks host tools and high-level configuration consistency.

Current checks:

- `cmake`, `ninja`, and `python3` availability in `PATH`
- `FLASH_MGR_ENABLE` consistency with ARM target switches
- `FRAMEWORK_USE_LFS` and `FRAMEWORK_USE_UART` when Flash Manager is enabled

Example:

```text
[OK] cmake: /usr/bin/cmake
[OK] ninja: /usr/bin/ninja
[OK] python3: /usr/bin/python3
[ERR] flash-manager: FLASH_MGR_ENABLE requires FRAMEWORK_USE_UART
```

## Inspect

`inspect` prints the effective CMake flags forwarded from `config/config.yaml`.
Use it before a build when a target behaves differently than expected.

```text
arm (ARM, MinSizeRel, ninja)
  -DFRAMEWORK_USE_FREERTOS=ON
  -DFRAMEWORK_USE_LFS=ON
  -DFRAMEWORK_USE_UART=ON
```

## Size

`size` reads a linker `.map` file and prints memory region origins and lengths.
It is intentionally small in the first version; later versions can add section
and object-file ranking.
