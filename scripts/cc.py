#!/usr/bin/env python3
"""
YAML-driven build driver for the framework.

Reads config/config.yaml (a list of build targets) and runs cmake + build
for each target.  Each target gets its own build/ directory (e.g. build/arm/,
build/vm/).

Usage:
    python3 scripts/cc.py                  # build all targets
    python3 scripts/cc.py --target arm      # build only the named target
    python3 scripts/cc.py --target vm
"""

from __future__ import annotations

import argparse
import os
import pathlib
import shutil
import subprocess
import sys

import yaml

ROOT = pathlib.Path(__file__).resolve().parent.parent
CFG_PATH = ROOT / "config" / "config.yaml"
BUILD_ROOT = ROOT / "build"

GENERATOR_MAP = {"ninja": "Ninja", "make": "Unix Makefiles"}

META_KEYS = {"name", "platform", "build_type", "generator", "graphviz", "skip_syscfg", "runtime_mode"}
PATH_KEYS = {"arm_tool_chain_path", "sysconfig_path"}
STRING_KEYS = set()  # reserved for future non-boolean keys
RUNTIME_MODES = ("game", "flash_mgr", "test")


def load_targets() -> list[dict]:
    with CFG_PATH.open("r", encoding="utf-8") as f:
        cfg = yaml.safe_load(f) or {}
    targets = cfg.get("build")
    if not isinstance(targets, list):
        sys.exit(f"-- [cc.py] {CFG_PATH}: expected a `build:` list, got {type(targets).__name__}")
    return targets


def resolve_generator(value: str | None) -> str:
    if value is None or value == "auto":
        return "Ninja" if shutil.which("ninja") else "Unix Makefiles"
    if value in GENERATOR_MAP:
        return GENERATOR_MAP[value]
    sys.exit(f"-- [cc.py] unknown generator: {value!r}")


def _is_truthy(value: object) -> bool:
    if isinstance(value, bool):
        return value
    if isinstance(value, (int, float)):
        return value != 0
    if isinstance(value, str):
        return value.strip().upper() in ("ON", "YES", "TRUE", "1")
    return False


def resolve_runtime_mode(target: dict) -> str:
    mode = str(target.get("runtime_mode", "game")).strip()
    if mode not in RUNTIME_MODES:
        name = target.get("name", "<unnamed>")
        allowed = ", ".join(RUNTIME_MODES)
        sys.exit(
            f"-- [cc.py] target {name!r}: invalid runtime_mode {mode!r}; "
            f"expected one of: {allowed}"
        )
    return mode


def validate_runtime_dependencies(target: dict, mode: str) -> None:
    if mode != "flash_mgr":
        return
    missing = [
        key
        for key in ("FRAMEWORK_USE_LFS", "FRAMEWORK_USE_UART")
        if not _is_truthy(target.get(key))
    ]
    if missing:
        name = target.get("name", "<unnamed>")
        sys.exit(
            f"-- [cc.py] target {name!r}: runtime_mode 'flash_mgr' requires "
            + " and ".join(f"{key}=ON" for key in missing)
        )


def resolve_config_path(
    value: object,
    default_relative: str,
    root: pathlib.Path = ROOT,
) -> pathlib.Path:
    text = "" if value is None else str(value).strip()
    path = pathlib.Path(default_relative) if text == "" else pathlib.Path(text)
    if not path.is_absolute():
        path = root / path
    return path


def cmake_cache_args(target: dict, root: pathlib.Path = ROOT) -> list[str]:
    args: list[str] = []
    is_arm = str(target.get("platform", "ARM")).upper() == "ARM"
    runtime_mode = resolve_runtime_mode(target)
    validate_runtime_dependencies(target, runtime_mode)
    args.append(f"-DFRAMEWORK_RUNTIME_MODE={runtime_mode}")

    if is_arm:
        args.append(
            "-DARM_TOOLCHAIN_ROOT="
            + str(resolve_config_path(target.get("arm_tool_chain_path"), "tools/gcc-arm-none-eabi", root))
        )
        args.append(
            "-DSYSCONFIG_ROOT="
            + str(resolve_config_path(target.get("sysconfig_path"), "tools/sysconfig", root))
        )
        args.append(
            "-DSKIP_SYSCFG=" + ("ON" if _is_truthy(target.get("skip_syscfg")) else "OFF")
        )

    for key, value in target.items():
        if key in META_KEYS or key in PATH_KEYS:
            continue
        if key in STRING_KEYS:
            args.append(f"-D{key}={value}")
        else:
            args.append(f"-D{key}={'ON' if _is_truthy(value) else 'OFF'}")
    return args


def build_one(target: dict) -> None:
    name = target.get("name")
    if not name:
        sys.exit("-- [cc.py] every build target needs a `name:`")

    build_dir = BUILD_ROOT / name
    build_dir.mkdir(parents=True, exist_ok=True)

    build_type = target.get("build_type", "RelWithDebInfo")
    generator = resolve_generator(target.get("generator"))
    graphviz = _is_truthy(target.get("graphviz", False))

    cmake_cmd: list[str] = [
        "cmake", "-G", generator,
        f"-DCMAKE_BUILD_TYPE={build_type}",
        f"-DBUILD_PLATFORM={target.get('platform', 'ARM')}",
    ]

    cmake_cmd.extend(cmake_cache_args(target))

    if graphviz:
        cmake_cmd.append("--graphviz=framework.dot")

    cmake_cmd.append("../..")   # source dir (build/<name>/ → root)

    print(f"\n-- [cc.py] === target '{name}' | {build_type} | {generator} ===")
    print(f"-- [cc.py] $ (cd {build_dir} && {' '.join(cmake_cmd)})")
    subprocess.run(cmake_cmd, cwd=build_dir, check=True)

    nproc = str(os.cpu_count() or 1)
    build_cmd = ["cmake", "--build", ".", "-j", nproc]
    print(f"-- [cc.py] $ (cd {build_dir} && {' '.join(build_cmd)})")
    subprocess.run(build_cmd, cwd=build_dir, check=True)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--target", help="Build only the named target")
    args = parser.parse_args()

    targets = load_targets()
    if args.target:
        targets = [t for t in targets if t.get("name") == args.target]
        if not targets:
            sys.exit(f"-- [cc.py] no target named '{args.target}'")

    for t in targets:
        build_one(t)


if __name__ == "__main__":
    main()
