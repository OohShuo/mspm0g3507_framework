import tempfile
import unittest
from unittest import mock
from pathlib import Path

from scripts.framework import (
    CheckResult,
    check_arm_toolchain,
    check_python_package,
    check_sysconfig_tool,
    load_targets,
    parse_map_summary,
    resolve_arm_toolchain_path,
    resolve_config_path,
    resolve_sysconfig_path,
    summarize_target,
)


class FrameworkToolTests(unittest.TestCase):
    def test_load_targets_reads_build_list(self):
        with tempfile.TemporaryDirectory() as tmp:
            cfg = Path(tmp) / "config.yaml"
            cfg.write_text(
                "build:\n"
                "  - name: vm\n"
                "    platform: VM\n"
                "    FRAMEWORK_USE_LFS: ON\n",
                encoding="utf-8",
            )

            targets = load_targets(cfg)

        self.assertEqual(targets[0]["name"], "vm")
        self.assertEqual(targets[0]["platform"], "VM")

    def test_load_targets_reports_missing_pyyaml(self):
        with tempfile.TemporaryDirectory() as tmp:
            cfg = Path(tmp) / "config.yaml"
            cfg.write_text("build: []\n", encoding="utf-8")

            with mock.patch("scripts.framework.importlib.import_module", side_effect=ImportError):
                with self.assertRaisesRegex(RuntimeError, "PyYAML"):
                    load_targets(cfg)

    def test_summarize_target_lists_forwarded_cmake_flags(self):
        target = {
            "name": "arm",
            "platform": "ARM",
            "build_type": "MinSizeRel",
            "generator": "ninja",
            "FRAMEWORK_USE_LFS": "ON",
        }

        summary = summarize_target(target)

        self.assertEqual(summary["name"], "arm")
        self.assertIn("-DFRAMEWORK_USE_LFS=ON", summary["cmake_flags"])

    def test_config_paths_resolve_empty_relative_and_absolute_values(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            absolute = root / "external" / "sysconfig"

            self.assertEqual(
                resolve_config_path("", "tools/gcc-arm-none-eabi", root),
                root / "tools" / "gcc-arm-none-eabi",
            )
            self.assertEqual(
                resolve_config_path("vendor/arm-gcc", "tools/gcc-arm-none-eabi", root),
                root / "vendor" / "arm-gcc",
            )
            self.assertEqual(
                resolve_config_path(str(absolute), "tools/sysconfig", root),
                absolute,
            )

    def test_arm_and_sysconfig_paths_use_named_target_keys(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            target = {
                "arm_tool_chain_path": "vendor/arm-gcc",
                "sysconfig_path": "vendor/ti-sysconfig",
            }

            self.assertEqual(resolve_arm_toolchain_path(target, root), root / "vendor" / "arm-gcc")
            self.assertEqual(resolve_sysconfig_path(target, root), root / "vendor" / "ti-sysconfig")

    def test_check_result_status_distinguishes_optional_warnings(self):
        self.assertEqual(CheckResult("x", True, "ok").status, "OK")
        self.assertEqual(CheckResult("x", False, "missing", required=True).status, "ERR")
        self.assertEqual(CheckResult("x", False, "missing", required=False).status, "WARN")

    def test_python_package_check_reports_optional_dependency_as_warning(self):
        result = check_python_package(
            "module_that_should_not_exist_for_framework_doctor",
            "fake-package",
            required=False,
            when="测试可选依赖缺失",
        )

        self.assertFalse(result.ok)
        self.assertFalse(result.required)
        self.assertEqual(result.status, "WARN")
        self.assertIn("必要性：可选", result.message)
        self.assertIn("何时需要：测试可选依赖缺失", result.message)

    def test_arm_toolchain_check_uses_resolved_toolchain_root(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            toolchain = root / "custom-arm"
            bin_dir = toolchain / "bin"
            bin_dir.mkdir(parents=True)
            for executable in (
                "arm-none-eabi-gcc",
                "arm-none-eabi-g++",
                "arm-none-eabi-objcopy",
                "arm-none-eabi-size",
            ):
                (bin_dir / executable).write_text("#!/bin/sh\n", encoding="utf-8")

            results = check_arm_toolchain(
                {"name": "arm", "arm_tool_chain_path": "custom-arm"},
                root,
            )

        self.assertEqual([result.status for result in results], ["OK", "OK", "OK", "OK"])
        self.assertTrue(all("必要性：必需" in result.message for result in results))
        self.assertTrue(all("何时需要：构建 ARM target 'arm'" in result.message for result in results))

    def test_sysconfig_check_accepts_cli_script_in_resolved_root(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            sysconfig = root / "custom-sysconfig"
            sysconfig.mkdir()
            (sysconfig / "sysconfig_cli.sh").write_text("#!/bin/sh\n", encoding="utf-8")

            result = check_sysconfig_tool(
                {"name": "arm", "sysconfig_path": "custom-sysconfig"},
                root,
            )

        self.assertTrue(result.ok)
        self.assertIn("必要性：可选", result.message)
        self.assertIn("何时需要：重新生成 SysConfig 文件", result.message)

    def test_parse_map_summary_extracts_memory_lines(self):
        text = """
Memory Configuration

Name             Origin             Length             Attributes
FLASH            0x00000000         0x00020000         xr
SRAM             0x20000000         0x00008000         xrw
"""

        rows = parse_map_summary(text)

        self.assertEqual(rows["FLASH"], {"origin": "0x00000000", "length": "0x00020000"})
        self.assertEqual(rows["SRAM"], {"origin": "0x20000000", "length": "0x00008000"})


if __name__ == "__main__":
    unittest.main()
