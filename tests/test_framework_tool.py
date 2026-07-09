import tempfile
import unittest
from pathlib import Path

from scripts.framework import (
    check_flash_manager_config,
    load_targets,
    parse_map_summary,
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

    def test_flash_manager_requires_lfs_uart_and_app_switch(self):
        target = {
            "name": "arm",
            "FRAMEWORK_USE_LFS": "ON",
            "FRAMEWORK_USE_UART": "OFF",
        }
        app_config = "#define FLASH_MGR_ENABLE 1\n"

        result = check_flash_manager_config(target, app_config)

        self.assertFalse(result.ok)
        self.assertIn("FRAMEWORK_USE_UART", result.message)

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
