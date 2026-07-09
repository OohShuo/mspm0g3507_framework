import tempfile
import unittest
from pathlib import Path

from scripts.cc import cmake_cache_args, resolve_config_path


class CcConfigPathTests(unittest.TestCase):
    def test_resolve_config_path_uses_default_relative_to_root(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)

            resolved = resolve_config_path("", "tools/gcc-arm-none-eabi", root)

        self.assertEqual(resolved, root / "tools" / "gcc-arm-none-eabi")

    def test_resolve_config_path_keeps_absolute_value(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            external = root / "external" / "sysconfig"

            resolved = resolve_config_path(str(external), "tools/sysconfig", root)

        self.assertEqual(resolved, external)

    def test_arm_target_forwards_tool_roots_without_boolean_path_flags(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            target = {
                "name": "arm",
                "platform": "ARM",
                "arm_tool_chain_path": "vendor/arm-gcc",
                "sysconfig_path": "vendor/ti-sysconfig",
                "FRAMEWORK_USE_LFS": "ON",
            }

            args = cmake_cache_args(target, root)

        self.assertIn(f"-DARM_TOOLCHAIN_ROOT={root / 'vendor' / 'arm-gcc'}", args)
        self.assertIn(f"-DSYSCONFIG_ROOT={root / 'vendor' / 'ti-sysconfig'}", args)
        self.assertIn("-DFRAMEWORK_USE_LFS=ON", args)
        self.assertNotIn("-Darm_tool_chain_path=OFF", args)
        self.assertNotIn("-Dsysconfig_path=OFF", args)

    def test_vm_target_does_not_forward_arm_tool_roots(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            target = {
                "name": "vm",
                "platform": "VM",
                "arm_tool_chain_path": "vendor/arm-gcc",
                "sysconfig_path": "vendor/ti-sysconfig",
                "FRAMEWORK_USE_LFS": "ON",
            }

            args = cmake_cache_args(target, root)

        self.assertNotIn(f"-DARM_TOOLCHAIN_ROOT={root / 'vendor' / 'arm-gcc'}", args)
        self.assertNotIn(f"-DSYSCONFIG_ROOT={root / 'vendor' / 'ti-sysconfig'}", args)
        self.assertIn("-DFRAMEWORK_USE_LFS=ON", args)


if __name__ == "__main__":
    unittest.main()
