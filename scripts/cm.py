#!/usr/bin/env python3
"""
YAML-driven build driver for the framework.

Reads config/config.yaml and runs cmake + the build step, forwarding every
key under `build:` (except the three meta keys `build_type`, `generator`,
`graphviz`) verbatim as -D<key>=<value> to cmake. The root CMakeLists.txt
turns each cache var into a 0/1 global compile def so .c code can
#if FRAMEWORK_USE_<NAME> on it.

Usage:
    python3 scripts/cm.py

Edit config/config.yaml to change the profile. No CLI flags for v1 — the
yaml is the single source of truth.
"""

from __future__ import annotations

import os
import pathlib
import shutil
import subprocess
import sys

import yaml

# ----------------------------------------------------------------------- paths

ROOT = pathlib.Path(__file__).resolve().parent.parent
CFG_PATH = ROOT / "config" / "config.yaml"

# cmake -G values for the supported yaml `generator:` values.
GENERATOR_MAP = {
    "ninja": "Ninja",
    "make": "Unix Makefiles",
}

# Keys handled by this script itself, NOT forwarded to cmake as -D.
META_KEYS = {"build_type", "generator", "graphviz"}


def load_config() -> dict:
    """Load and return the `build:` dict from config/config.yaml."""
    with CFG_PATH.open("r", encoding="utf-8") as f:
        cfg = yaml.safe_load(f) or {}
    build = cfg.get("build")
    if not isinstance(build, dict):
        sys.exit(f"-- [cm.py] {CFG_PATH}: expected a `build:` dict, got {type(build).__name__}")
    return build


def resolve_generator(yaml_value: str | None) -> str:
    """Map yaml `generator:` to a cmake -G value. 'auto' probes the host."""
    if yaml_value is None:
        pass  # fall through to auto
    elif yaml_value in GENERATOR_MAP:
        return GENERATOR_MAP[yaml_value]
    elif yaml_value != "auto":
        sys.exit(
            f"-- [cm.py] unknown generator: {yaml_value!r} "
            f"(expected one of: {', '.join(sorted(GENERATOR_MAP))}, auto)"
        )
    # auto / None: prefer ninja if installed, else fall back to make.
    if shutil.which("ninja"):
        return "Ninja"
    return "Unix Makefiles"


def _is_truthy(value: object) -> bool:
    """Coerce a yaml value to a boolean.

    yaml parses `ON`/`OFF`/`YES`/`NO`/`TRUE`/`FALSE` and bare numbers as
    native Python bool/int. Strings like `"on"` or `"yes"` should also
    count as on. Anything else is off.
    """
    if isinstance(value, bool):
        return value
    if isinstance(value, (int, float)):
        return value != 0
    if isinstance(value, str):
        return value.strip().upper() in ("ON", "YES", "TRUE", "1")
    return False


def build_cmake_command(cfg: dict) -> list[str]:
    """Construct the cmake invocation from the parsed yaml dict."""
    build_type = cfg.get("build_type")
    if not build_type:
        sys.exit("-- [cm.py] `build_type:` is required in config.yaml")

    generator = resolve_generator(cfg.get("generator"))
    graphviz = _is_truthy(cfg.get("graphviz", False))

    cmd: list[str] = [
        "cmake",
        "-G", generator,
        f"-DCMAKE_BUILD_TYPE={build_type}",
    ]

    # Forward every non-meta key as -D<key>=ON|OFF. Order matches the
    # yaml file order, stable enough for reproducible logs.
    for key, value in cfg.items():
        if key in META_KEYS:
            continue
        flag = "ON" if _is_truthy(value) else "OFF"
        cmd.append(f"-D{key}={flag}")

    # graphviz goes between the -D flags and the source-dir `..`. cmake
    # accepts it on either side, but keeping the source dir last makes
    # the command visually scan-friendly.
    if graphviz:
        cmd.append("--graphviz=framework.dot")

    cmd.append("..")
    return cmd


def run(cmd: list[str], cwd: pathlib.Path) -> None:
    """Echo + run a command; non-zero exit aborts the script."""
    print(f"-- [cm.py] $ (cd {cwd} && {' '.join(cmd)})")
    subprocess.run(cmd, cwd=cwd, check=True)


def main() -> None:
    cfg = load_config()

    # Use separate build dir for VM to avoid ARM/x86 conflicts.
    build_dir = ROOT / ("build_vm" if _is_truthy(cfg.get("FRAMEWORK_VIRTUAL_DEVICE")) else "build")

    cmake_cmd = build_cmake_command(cfg)

    build_dir.mkdir(parents=True, exist_ok=True)
    run(cmake_cmd, build_dir)

    # Generator-agnostic build: avoids the trap of trying to spawn
    # `unix makefiles` when the generator is "Unix Makefiles".
    nproc = str(os.cpu_count() or 1)
    build_cmd = ["cmake", "--build", ".", "-j", nproc]
    run(build_cmd, build_dir)


if __name__ == "__main__":
    main()
