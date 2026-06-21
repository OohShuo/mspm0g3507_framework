import unittest
from pathlib import Path


class PlatformBootstrapTest(unittest.TestCase):
    def test_main_is_platform_neutral(self):
        source = Path("src/main.c").read_text()
        self.assertIn('#include "platform.h"', source)
        self.assertIn("Platform_Init()", source)
        self.assertIn("Platform_Start()", source)
        for forbidden in ("SDL", "SYSCFG_DL_init", "vTaskStartScheduler", '"task.h"'):
            self.assertNotIn(forbidden, source)

    def test_platform_implementations_own_platform_lifecycle(self):
        arm = Path("src/platform/platform_arm.c").read_text()
        vm = Path("src/platform/platform_vm.c").read_text()
        self.assertIn("SYSCFG_DL_init();", arm)
        self.assertIn("vTaskStartScheduler();", arm)
        self.assertIn("Vm_Freertos_Start_Tasks()", vm)
        self.assertIn("SDL_PollEvent", vm)

    def test_legacy_vm_entry_is_removed(self):
        self.assertFalse(Path("src/vm/main_vm.c").exists())


if __name__ == "__main__":
    unittest.main()
