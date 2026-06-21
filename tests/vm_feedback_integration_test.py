import unittest
from pathlib import Path


class VmFeedbackIntegrationTest(unittest.TestCase):
    def test_main_loop_routes_haptics_lifecycle(self):
        source = Path("src/vm/main_vm.c").read_text()
        self.assertIn("Vm_Haptics_Init();", source)
        self.assertIn("Vm_Haptics_Handle_Event(&e);", source)
        self.assertIn("Vm_Haptics_Update();", source)
        self.assertIn("Vm_Haptics_Deinit();", source)

    def test_display_renders_vibration_overlay(self):
        source = Path("src/vm/display_vm.c").read_text()
        self.assertIn("Vm_Haptics_Get_Strength()", source)
        self.assertIn("draw_vibration_overlay", source)


if __name__ == "__main__":
    unittest.main()
