import pathlib
import re
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[2]
EXPECTED = [
    "pacman",
    "snake",
    "tank_battle",
    "air_battle",
    "tetris",
    "breakout",
    "pong",
    "gomoku",
    "game_2048",
    "dino_runner",
    "flappy_bird",
    "maze",
    "needle",
    "dodge_box",
    "rhythm",
    "sfx_lib",
    "calculator",
    "fps_test",
    "volume_control",
    "bad_apple",
    "info",
]


class GameEntryRegistryTest(unittest.TestCase):
    def entry_source(self, token):
        return ROOT / "src/app/games" / token / f"{token}.c"

    def assert_entry(self, token):
        text = self.entry_source(token).read_text()
        self.assertEqual(len(re.findall(rf"const Game_descriptor\s+game_{token}_entry\s*=", text)), 1)
        self.assertIn(f".id = game_id_{token}", text)
        self.assertRegex(text, r"\.draw_icon\s*=\s*\w+")
        self.assertRegex(text, r"\.name_color\s*=\s*[^,]+")
        self.assertRegex(text, r"\.init\s*=\s*\w+")
        self.assertRegex(text, r"\.update\s*=\s*\w+")
        self.assertRegex(text, r"\.get_score\s*=\s*\w+")
        self.assertRegex(text, r"\.is_game\s*=\s*[01]")

    def test_entry_list_has_canonical_order(self):
        text = (ROOT / "src/app/game_console/game_entries.inc").read_text()
        self.assertEqual(re.findall(r"^game_entry\((\w+)\)", text, re.MULTILINE), EXPECTED)

    def test_ids_are_generated_from_entry_list(self):
        header = (ROOT / "src/app/game_console/game_registry.h").read_text()
        self.assertIn("#define game_entry(name) game_id_##name,", header)
        self.assertNotIn("game_id_racing", header)

    def test_first_entry_batch(self):
        for token in EXPECTED[:7]:
            self.assert_entry(token)

    def test_second_entry_batch(self):
        for token in EXPECTED[7:14]:
            self.assert_entry(token)

    def test_tool_entry_batch(self):
        for token in EXPECTED[14:]:
            self.assert_entry(token)

    def test_registry_array_is_generated(self):
        source = (ROOT / "src/app/game_console/game_registry.c").read_text()
        self.assertIn("#define game_entry(name) &game_##name##_entry,", source)

    def test_console_has_no_entry_specific_icons(self):
        source = (ROOT / "src/app/game_console/game_console.c").read_text()
        self.assertNotRegex(source, r"game_icon_|draw_\w+_icon")
        self.assertIn("game->draw_icon(g_lcd, icon_x, icon_y);", source)

    def test_primary_entry_headers_are_removed(self):
        for token in EXPECTED:
            self.assertFalse((ROOT / "src/app/games" / token / f"{token}.h").exists())

    def test_only_descriptors_keep_external_linkage(self):
        for token in EXPECTED:
            text = self.entry_source(token).read_text()
            self.assertNotRegex(text, r"\n(?:void|Game_result|uint32_t)\s+[A-Z]\w+_(?:Init|Update|Get_Score)\s*\(")
            self.assertEqual(text.count(f"const Game_descriptor game_{token}_entry"), 1)

    def test_resource_headers_remain(self):
        self.assertTrue((ROOT / "src/app/games/air_battle/air_assets.h").exists())
        self.assertTrue((ROOT / "src/app/games/info/info_image_hitsz.h").exists())


if __name__ == "__main__":
    unittest.main()
