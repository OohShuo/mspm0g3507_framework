#!/usr/bin/env python3
"""Developer diagnostics for the MSPM0G3507 framework."""

from __future__ import annotations

import argparse
import re
import shutil
from dataclasses import dataclass
from pathlib import Path

import yaml

ROOT = Path(__file__).resolve().parent.parent
CONFIG_PATH = ROOT / "config" / "config.yaml"
APP_CONFIG_PATH = ROOT / "config" / "app_config.h"

META_KEYS = {"name", "platform", "build_type", "generator", "graphviz"}


@dataclass
class CheckResult:
    name: str
    ok: bool
    message: str


def _truthy(value: object) -> bool:
    if isinstance(value, bool):
        return value
    if isinstance(value, (int, float)):
        return value != 0
    if isinstance(value, str):
        return value.strip().upper() in {"ON", "YES", "TRUE", "1"}
    return False


def load_targets(path: Path = CONFIG_PATH) -> list[dict]:
    data = yaml.safe_load(path.read_text(encoding="utf-8")) or {}
    targets = data.get("build")
    if not isinstance(targets, list):
        raise ValueError(f"{path}: expected a `build:` list")
    return targets


def _macro_enabled(text: str, macro: str) -> bool:
    pattern = rf"^\s*#\s*define\s+{re.escape(macro)}\s+([01]|ON|OFF|TRUE|FALSE)\b"
    for line in text.splitlines():
        match = re.match(pattern, line, re.IGNORECASE)
        if match:
            return _truthy(match.group(1))
    return False


def check_flash_manager_config(target: dict, app_config_text: str) -> CheckResult:
    if not _macro_enabled(app_config_text, "FLASH_MGR_ENABLE"):
        return CheckResult("flash-manager", True, "FLASH_MGR_ENABLE is disabled")

    missing = []
    for key in ("FRAMEWORK_USE_LFS", "FRAMEWORK_USE_UART"):
        if not _truthy(target.get(key)):
            missing.append(key)

    if missing:
        return CheckResult(
            "flash-manager",
            False,
            "FLASH_MGR_ENABLE requires " + ", ".join(missing),
        )
    return CheckResult("flash-manager", True, "Flash Manager switches are consistent")


def summarize_target(target: dict) -> dict:
    flags = []
    for key, value in sorted(target.items()):
        if key in META_KEYS:
            continue
        flags.append(f"-D{key}={'ON' if _truthy(value) else 'OFF'}")
    return {
        "name": target.get("name", "<unnamed>"),
        "platform": target.get("platform", "ARM"),
        "build_type": target.get("build_type", "RelWithDebInfo"),
        "generator": target.get("generator", "auto"),
        "cmake_flags": flags,
    }


def parse_map_summary(text: str) -> dict[str, dict[str, str]]:
    rows: dict[str, dict[str, str]] = {}
    for line in text.splitlines():
        parts = line.split()
        if len(parts) >= 3 and parts[1].startswith("0x") and parts[2].startswith("0x"):
            rows[parts[0]] = {"origin": parts[1], "length": parts[2]}
    return rows


def _tool_check(name: str, executable: str) -> CheckResult:
    found = shutil.which(executable)
    return CheckResult(name, bool(found), found or f"{executable} not found in PATH")


def cmd_doctor(_: argparse.Namespace) -> int:
    app_text = APP_CONFIG_PATH.read_text(encoding="utf-8") if APP_CONFIG_PATH.exists() else ""
    checks = [
        _tool_check("cmake", "cmake"),
        _tool_check("ninja", "ninja"),
        _tool_check("python3", "python3"),
    ]
    for target in load_targets():
        if str(target.get("platform", "")).upper() == "ARM":
            checks.append(check_flash_manager_config(target, app_text))

    exit_code = 0
    for check in checks:
        status = "OK" if check.ok else "ERR"
        print(f"[{status}] {check.name}: {check.message}")
        if not check.ok:
            exit_code = 1
    return exit_code


def cmd_inspect(_: argparse.Namespace) -> int:
    for target in load_targets():
        summary = summarize_target(target)
        print(f"{summary['name']} ({summary['platform']}, {summary['build_type']}, {summary['generator']})")
        for flag in summary["cmake_flags"]:
            print(f"  {flag}")
    return 0


def cmd_size(args: argparse.Namespace) -> int:
    map_path = Path(args.map_file)
    rows = parse_map_summary(map_path.read_text(encoding="utf-8"))
    for name, row in rows.items():
        print(f"{name}: origin={row['origin']} length={row['length']}")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)
    sub.add_parser("doctor").set_defaults(func=cmd_doctor)
    sub.add_parser("inspect").set_defaults(func=cmd_inspect)
    size = sub.add_parser("size")
    size.add_argument("map_file")
    size.set_defaults(func=cmd_size)
    args = parser.parse_args()
    return args.func(args)


if __name__ == "__main__":
    raise SystemExit(main())
