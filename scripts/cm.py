#!/usr/bin/env python3
"""
YAML-driven build driver for the framework.

Reads config/config.yaml (a list of build targets) and runs cmake + build
for each target.  Each target gets its own build/ directory (e.g. build/arm/,
build/vm/).

Usage:
    python3 scripts/cm.py                  # build all targets
    python3 scripts/cm.py --target arm      # build only the named target
    python3 scripts/cm.py --target vm
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

META_KEYS = {"name", "platform", "build_type", "generator", "graphviz"}
STRING_KEYS = set()  # reserved for future non-boolean keys


def load_targets() -> list[dict]:
    with CFG_PATH.open("r", encoding="utf-8") as f:
        cfg = yaml.safe_load(f) or {}
    targets = cfg.get("build")
    if not isinstance(targets, list):
        sys.exit(f"-- [cm.py] {CFG_PATH}: expected a `build:` list, got {type(targets).__name__}")
    return targets


def resolve_generator(value: str | None) -> str:
    if value is None or value == "auto":
        return "Ninja" if shutil.which("ninja") else "Unix Makefiles"
    if value in GENERATOR_MAP:
        return GENERATOR_MAP[value]
    sys.exit(f"-- [cm.py] unknown generator: {value!r}")


def _is_truthy(value: object) -> bool:
    if isinstance(value, bool):
        return value
    if isinstance(value, (int, float)):
        return value != 0
    if isinstance(value, str):
        return value.strip().upper() in ("ON", "YES", "TRUE", "1")
    return False


def build_one(target: dict) -> None:
    name = target.get("name")
    if not name:
        sys.exit("-- [cm.py] every build target needs a `name:`")

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

    # Forward remaining keys as -D<key>=ON|OFF (or verbatim for STRING_KEYS)
    for key, value in target.items():
        if key in META_KEYS:
            continue
        if key in STRING_KEYS:
            cmake_cmd.append(f"-D{key}={value}")
        else:
            cmake_cmd.append(f"-D{key}={'ON' if _is_truthy(value) else 'OFF'}")

    if graphviz:
        cmake_cmd.append("--graphviz=framework.dot")

    cmake_cmd.append("../..")   # source dir (build/<name>/ → root)

    print(f"\n-- [cm.py] === target '{name}' | {build_type} | {generator} ===")
    print(f"-- [cm.py] $ (cd {build_dir} && {' '.join(cmake_cmd)})")
    subprocess.run(cmake_cmd, cwd=build_dir, check=True)

    nproc = str(os.cpu_count() or 1)
    build_cmd = ["cmake", "--build", ".", "-j", nproc]
    print(f"-- [cm.py] $ (cd {build_dir} && {' '.join(build_cmd)})")
    subprocess.run(build_cmd, cwd=build_dir, check=True)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--target", help="Build only the named target")
    args = parser.parse_args()

    targets = load_targets()
    if args.target:
        targets = [t for t in targets if t.get("name") == args.target]
        if not targets:
            sys.exit(f"-- [cm.py] no target named '{args.target}'")

    for t in targets:
        build_one(t)


if __name__ == "__main__":
    main()
