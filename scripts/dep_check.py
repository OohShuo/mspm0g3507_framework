#!/usr/bin/env python3
"""Developer dependency check for the MSPM0G3507 framework."""

from __future__ import annotations

import importlib.util
import os
import re
import shutil
import unicodedata
from dataclasses import dataclass
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CONFIG_PATH = ROOT / "config" / "config.yaml"
META_KEYS = {"name", "platform", "build_type", "generator", "graphviz", "skip_syscfg"}
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


_ANSI_RE = re.compile(r"\033\[[0-9;]*m")


def _display_width(s: str) -> int:
    """Monospace display width accounting for CJK full-width characters."""
    w = 0
    for ch in _ANSI_RE.sub("", str(s)):
        w += 2 if unicodedata.east_asian_width(ch) in ("W", "F") else 1
    return w


def _pad(s: str, width: int) -> str:
    """Left-pad with spaces so the string occupies *width* monospace columns."""
    return s + " " * max(0, width - _display_width(s))


def _print_table(headers: list[str], rows: list[list[str]]) -> None:
    """Print a compact aligned table with a separator line under the header."""
    if not rows:
        return
    widths = [
        max(_display_width(h), max((_display_width(c) for c in col), default=0))
        for h, col in zip(headers, zip(*rows))
    ]
    sep = "  ".join("─" * w for w in widths)
    header_line = "  ".join(_pad(h, w) for h, w in zip(headers, widths))
    print(header_line)
    print(sep)
    for row in rows:
        print("  ".join(_pad(c, w) for c, w in zip(row, widths)))


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


def run_checks() -> int:
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

    exit_code = 0
    icon = {
        "OK": "\033[32m✓\033[0m",
        "ERR": "\033[31m✗\033[0m",
        "WARN": "\033[33m⚠\033[0m",
    }
    rows: list[list[str]] = []
    for check in checks:
        note = check.when if check.status != "OK" else ""
        rows.append([icon[check.status], check.name, note])
        if check.status == "ERR":
            exit_code = 1
    _print_table(["", "Check", "Note"], rows)
    return exit_code


if __name__ == "__main__":
    raise SystemExit(run_checks())
