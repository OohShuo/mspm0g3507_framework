from __future__ import annotations

import importlib.util
import pathlib
import re
import subprocess
import tempfile
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("framework_cc", ROOT / "scripts" / "cc.py")
assert SPEC is not None and SPEC.loader is not None
CC = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(CC)


class RuntimeModeTests(unittest.TestCase):
    def cache_args(self, **overrides: object) -> list[str]:
        target = {
            "name": "unit",
            "platform": "VM",
            "FRAMEWORK_USE_LFS": "ON",
            "FRAMEWORK_USE_UART": "ON",
        }
        target.update(overrides)
        return CC.cmake_cache_args(target, ROOT)

    def test_missing_mode_defaults_to_game(self) -> None:
        self.assertIn("-DFRAMEWORK_RUNTIME_MODE=game", self.cache_args())

    def test_valid_modes_are_passed_as_strings(self) -> None:
        for mode in ("game", "flash_mgr", "test"):
            with self.subTest(mode=mode):
                args = self.cache_args(runtime_mode=mode)
                self.assertIn(f"-DFRAMEWORK_RUNTIME_MODE={mode}", args)
                self.assertFalse(any(arg.startswith("-Druntime_mode=") for arg in args))

    def test_invalid_mode_is_rejected(self) -> None:
        with self.assertRaises(SystemExit) as raised:
            self.cache_args(runtime_mode="console")
        self.assertIn("game, flash_mgr, test", str(raised.exception))

    def test_dependency_validation_is_deferred_to_config_headers(self) -> None:
        args = self.cache_args(
            runtime_mode="flash_mgr",
            FRAMEWORK_USE_LFS="OFF",
            FRAMEWORK_USE_UART="OFF",
        )
        self.assertIn("-DFRAMEWORK_RUNTIME_MODE=flash_mgr", args)

class RuntimeHeaderTests(unittest.TestCase):
    def macros_for(self, current: int) -> dict[str, str]:
        result = subprocess.run(
            [
                "cc",
                "-dM",
                "-E",
                "-x",
                "c",
                "-DFRAMEWORK_RUNTIME_MODE_GAME=1",
                "-DFRAMEWORK_RUNTIME_MODE_FLASH_MGR=2",
                "-DFRAMEWORK_RUNTIME_MODE_TEST=3",
                f"-DFRAMEWORK_RUNTIME_MODE_CURRENT={current}",
                "-DFRAMEWORK_USE_FREERTOS=1",
                "-DFRAMEWORK_USE_LFS=1",
                "-DFRAMEWORK_USE_UART=1",
                "-include",
                str(ROOT / "config" / "app_config.h"),
                "/dev/null",
            ],
            check=True,
            capture_output=True,
            text=True,
        )
        macros: dict[str, str] = {}
        for line in result.stdout.splitlines():
            parts = line.split(maxsplit=2)
            if len(parts) == 3:
                macros[parts[1]] = parts[2]
        return macros

    def test_app_features_follow_runtime_mode(self) -> None:
        expected = {
            1: ("0", "1"),
            2: ("1", "0"),
            3: ("0", "0"),
        }
        for current, (flash_mgr, game) in expected.items():
            with self.subTest(current=current):
                macros = self.macros_for(current)
                self.assertEqual(flash_mgr, macros["FLASH_MGR_ENABLE"])
                self.assertEqual(game, macros["GAME_CONSOLE_ENABLE"])


class RuntimeIntegrationSourceTests(unittest.TestCase):
    def test_main_requires_test_mode_and_enabled_test(self) -> None:
        source = (ROOT / "src" / "main.c").read_text(encoding="utf-8")
        self.assertIn(
            "#if FRAMEWORK_RUNTIME_MODE_CURRENT == FRAMEWORK_RUNTIME_MODE_TEST && TEST_ANY_ENABLE",
            source,
        )

    def test_cmake_defines_and_validates_all_runtime_modes(self) -> None:
        source = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
        for mode in ("game", "flash_mgr", "test"):
            self.assertIn(f'FRAMEWORK_RUNTIME_MODE STREQUAL "{mode}"', source)
        self.assertIn("Unsupported FRAMEWORK_RUNTIME_MODE", source)
        self.assertIn("FRAMEWORK_RUNTIME_MODE_CURRENT=${FRAMEWORK_RUNTIME_MODE_CURRENT}", source)

    def test_user_guidance_does_not_request_manual_legacy_macro(self) -> None:
        cli = (ROOT / "scripts" / "flashmgr" / "cli.py").read_text(encoding="utf-8")
        self.assertNotIn("FLASH_MGR_ENABLE=1", cli)


class StaticConfigurationCheckTests(unittest.TestCase):
    def preprocess(self, header: str, *definitions: str) -> subprocess.CompletedProcess[str]:
        return subprocess.run(
            ["cc", "-E", "-x", "c", *(f"-D{item}" for item in definitions), "-include", str(ROOT / "config" / header), "/dev/null"],
            capture_output=True,
            text=True,
        )

    def test_flash_manager_rejects_missing_lfs_or_uart(self) -> None:
        base = (
            "FRAMEWORK_RUNTIME_MODE_GAME=1",
            "FRAMEWORK_RUNTIME_MODE_FLASH_MGR=2",
            "FRAMEWORK_RUNTIME_MODE_TEST=3",
            "FRAMEWORK_RUNTIME_MODE_CURRENT=2",
            "FRAMEWORK_USE_FREERTOS=1",
        )
        for available in ("FRAMEWORK_USE_LFS=1", "FRAMEWORK_USE_UART=1"):
            with self.subTest(available=available):
                result = self.preprocess("app_config.h", *base, available)
                self.assertNotEqual(0, result.returncode)

    def test_test_features_reject_missing_framework_dependencies(self) -> None:
        cases = {
            "TEST_LFS_ENABLE=1": "FRAMEWORK_USE_LFS",
            "TEST_LVGL_BALL_ENABLE=1": "FRAMEWORK_USE_LVGL",
            "TEST_LVGL_HELLO_ENABLE=1": "FRAMEWORK_USE_LVGL",
            "TEST_RTT_ENABLE=1": "FRAMEWORK_USE_RTT",
            "TEST_COM_UART_ENABLE=1": "FRAMEWORK_USE_UART",
            "TEST_SLIP_RECV_ENABLE=1": "FRAMEWORK_USE_UART",
        }
        for enabled_test, dependency in cases.items():
            with self.subTest(enabled_test=enabled_test):
                macro = enabled_test.split("=", maxsplit=1)[0]
                source = (ROOT / "config" / "test_config.h").read_text(encoding="utf-8")
                source = re.sub(
                    rf"^#define {re.escape(macro)}\s+0$",
                    f"#define {macro} 1",
                    source,
                    count=1,
                    flags=re.MULTILINE,
                )
                with tempfile.NamedTemporaryFile(mode="w", suffix=".h", encoding="utf-8") as header:
                    header.write(source)
                    header.flush()
                    result = subprocess.run(
                        ["cc", "-E", "-x", "c", "-include", header.name, "/dev/null"],
                        capture_output=True,
                        text=True,
                    )
                self.assertNotEqual(0, result.returncode)
                self.assertIn(dependency, result.stderr)


if __name__ == "__main__":
    unittest.main()
