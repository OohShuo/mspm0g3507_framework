import unittest
from pathlib import Path


class GameInfoConsoleTest(unittest.TestCase):
    def test_console_routes_menu_info_state(self):
        source = Path("src/app/game_console/game_console.c").read_text()
        self.assertIn("console_state_game_info", source)
        self.assertIn("if (input->x_pressed)", source)
        self.assertIn("static void update_game_info", source)
        self.assertIn("g_console_state == console_state_game_info", source)
        self.assertIn("Game_Info_Screen_Draw(", source)
        self.assertGreaterEqual(source.count("render_game_info();"), 2)
        self.assertIn('"A OK  X INFO  B BACK"', source)
        self.assertIn('"X: INFO"', source)


if __name__ == "__main__":
    unittest.main()
