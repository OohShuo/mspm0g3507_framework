import unittest
from pathlib import Path


class PlatformCMakeTest(unittest.TestCase):
    def test_platform_target_has_simple_name(self):
        source = Path("src/platform/CMakeLists.txt").read_text()
        self.assertIn("add_library(platform INTERFACE)", source)
        self.assertNotIn("framework_platform", source)

    def test_vm_is_static_library_without_main(self):
        source = Path("src/vm/CMakeLists.txt").read_text()
        self.assertIn("add_library(vm STATIC", source)
        self.assertNotIn("main_vm.c", source)

    def test_root_delegates_platform_composition(self):
        source = Path("CMakeLists.txt").read_text()
        self.assertIn("add_subdirectory(src/platform)", source)
        self.assertIn("platform_add_executable", source)
        self.assertNotIn("ENTRANCE_MAIN_C", source)

    def test_arm_interrupts_are_platform_owned_and_linked_directly(self):
        source = Path("src/platform/CMakeLists.txt").read_text()
        self.assertTrue(Path("src/platform/it_arm.c").exists())
        self.assertFalse(Path("src/it.c").exists())
        self.assertIn('"${PLATFORM_SOURCE_DIR}/it_arm.c"', source)


if __name__ == "__main__":
    unittest.main()
