#!/usr/bin/env python3
"""Developer diagnostics for the MSPM0G3507 framework."""

from __future__ import annotations

import argparse
import importlib.util
import os
import re
import shutil
from dataclasses import dataclass
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CONFIG_PATH = ROOT / "config" / "config.yaml"
APP_CONFIG_PATH = ROOT / "config" / "app_config.h"

META_KEYS = {"name", "platform", "build_type", "generator", "graphviz"}
PATH_KEYS = {"arm_tool_chain_path", "sysconfig_path"}


@dataclass
class CheckResult:
    name: str
    ok: bool
    message: str
    required: bool = True
    when: str = ""

    @property
    def status(self) -> str:
        if self.ok:
            return "OK"
        return "ERR" if self.required else "WARN"


def _truthy(value: object) -> bool:
    if isinstance(value, bool):
        return value
    if isinstance(value, (int, float)):
        return value != 0
    if isinstance(value, str):
        return value.strip().upper() in {"ON", "YES", "TRUE", "1"}
    return False


def load_targets(path: Path = CONFIG_PATH) -> list[dict]:
    try:
        yaml_module = importlib.import_module("yaml")
    except ImportError as exc:
        raise RuntimeError("PyYAML is required to parse config/config.yaml") from exc
    data = yaml_module.safe_load(path.read_text(encoding="utf-8")) or {}
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
    when = "FLASH_MGR_ENABLE=1 时"
    if not _macro_enabled(app_config_text, "FLASH_MGR_ENABLE"):
        return CheckResult(
            "flash-manager",
            True,
            "FLASH_MGR_ENABLE 已关闭；必要性：必需；何时需要：启用 Flash Manager 前检查开关一致性",
            when=when,
        )

    missing = []
    for key in ("FRAMEWORK_USE_LFS", "FRAMEWORK_USE_UART"):
        if not _truthy(target.get(key)):
            missing.append(key)

    if missing:
        return CheckResult(
            "flash-manager",
            False,
            "FLASH_MGR_ENABLE 需要 "
            + ", ".join(missing)
            + "；必要性：必需；何时需要：启用 Flash Manager 时",
            when=when,
        )
    return CheckResult(
        "flash-manager",
        True,
        "Flash Manager 开关一致；必要性：必需；何时需要：启用 Flash Manager 时",
        when=when,
    )


def summarize_target(target: dict) -> dict:
    flags = []
    for key, value in sorted(target.items()):
        if key in META_KEYS or key in PATH_KEYS:
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


def _need_text(required: bool, when: str) -> str:
    return f"必要性：{'必需' if required else '可选'}；何时需要：{when}"


def resolve_config_path(value: object, default_relative: str, root: Path = ROOT) -> Path:
    text = "" if value is None else str(value).strip()
    path = Path(default_relative) if text == "" else Path(text)
    if not path.is_absolute():
        path = root / path
    return path


def resolve_arm_toolchain_path(target: dict, root: Path = ROOT) -> Path:
    return resolve_config_path(target.get("arm_tool_chain_path"), "tools/gcc-arm-none-eabi", root)


def resolve_sysconfig_path(target: dict, root: Path = ROOT) -> Path:
    return resolve_config_path(target.get("sysconfig_path"), "tools/sysconfig", root)


def _tool_check(name: str, executable: str, required: bool, when: str) -> CheckResult:
    found = shutil.which(executable)
    message = (found or f"{executable} 未在 PATH 中找到") + "；" + _need_text(required, when)
    return CheckResult(name, bool(found), message, required=required, when=when)


def check_python_package(module: str, package_name: str, required: bool, when: str) -> CheckResult:
    found = importlib.util.find_spec(module) is not None
    message = (
        (f"{package_name} 已安装" if found else f"{package_name} 未安装")
        + "；"
        + _need_text(required, when)
    )
    return CheckResult(f"python package {package_name}", found, message, required=required, when=when)


def _executable_candidates(path: Path) -> list[Path]:
    suffixes = [""]
    if os.name == "nt":
        suffixes.append(".exe")
    return [path.with_name(path.name + suffix) for suffix in suffixes]


def _path_has_executable(path: Path) -> Path | None:
    for candidate in _executable_candidates(path):
        if candidate.exists():
            return candidate
    return None


def check_arm_toolchain(target: dict, root: Path = ROOT) -> list[CheckResult]:
    target_name = target.get("name", "<unnamed>")
    toolchain = resolve_arm_toolchain_path(target, root)
    bin_dir = toolchain / "bin"
    when = f"构建 ARM target '{target_name}'"
    checks: list[CheckResult] = []
    for executable in (
        "arm-none-eabi-gcc",
        "arm-none-eabi-g++",
        "arm-none-eabi-objcopy",
        "arm-none-eabi-size",
    ):
        path = bin_dir / executable
        found = _path_has_executable(path)
        message = (str(found) if found else f"{path} 不存在") + "；" + _need_text(True, when)
        checks.append(CheckResult(f"arm toolchain {executable}", bool(found), message, when=when))
    return checks


def check_sysconfig_tool(target: dict, root: Path = ROOT) -> CheckResult:
    sysconfig = resolve_sysconfig_path(target, root)
    when = "重新生成 SysConfig 文件"
    candidates = [
        sysconfig / "sysconfig_cli",
        sysconfig / "sysconfig_cli.sh",
        sysconfig / "sysconfig_cli.bat",
    ]
    found = next((candidate for candidate in candidates if candidate.exists()), None)
    message = (
        (str(found) if found else "未找到 sysconfig_cli、sysconfig_cli.sh 或 sysconfig_cli.bat")
        + "；"
        + _need_text(False, when)
    )
    return CheckResult("sysconfig-cli", bool(found), message, required=False, when=when)


def cmd_doctor(_: argparse.Namespace) -> int:
    app_text = APP_CONFIG_PATH.read_text(encoding="utf-8") if APP_CONFIG_PATH.exists() else ""
    checks = [
        _tool_check("python3", "python3", True, "运行项目脚本"),
        _tool_check("cmake", "cmake", True, "配置和生成所有构建 target"),
        _tool_check("ninja", "ninja", True, "generator 为 ninja 或 auto 选择 Ninja 时"),
        check_python_package("yaml", "PyYAML", True, "解析 config/config.yaml"),
        check_python_package("serial", "pyserial", False, "Flash Manager 和串口调试脚本"),
        check_python_package("PIL", "Pillow", False, "图片转换和 Flash Manager 图片上传"),
        check_python_package("numpy", "numpy", False, "视频和资源编码脚本"),
    ]
    try:
        targets = load_targets()
    except RuntimeError as exc:
        targets = []
        checks.append(
            CheckResult(
                "config/config.yaml",
                False,
                f"{exc}；{_need_text(True, '读取构建 target 配置')}",
            )
        )

    for target in targets:
        platform = str(target.get("platform", "")).upper()
        if platform == "VM":
            checks.append(_tool_check("sdl2-config", "sdl2-config", True, "构建 VM target"))
        if platform == "ARM":
            checks.extend(check_arm_toolchain(target))
            checks.append(check_sysconfig_tool(target))
            checks.append(check_flash_manager_config(target, app_text))

    exit_code = 0
    for check in checks:
        print(f"[{check.status}] {check.name}: {check.message}")
        if check.status == "ERR":
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
