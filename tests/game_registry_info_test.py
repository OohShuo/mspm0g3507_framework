import ast
import re
import unittest
from pathlib import Path


class GameRegistryInfoTest(unittest.TestCase):
    def test_every_entry_has_manually_wrapped_info(self):
        source = Path("src/app/game_console/game_registry.c").read_text()
        entries = re.findall(r"\{\s*(\.name\s*=.*?\n\s*)\},", source, re.S)
        self.assertEqual(len(entries), 20)

        for entry in entries:
            name = re.search(r'\.name\s*=\s*"([^"]+)"', entry).group(1)
            match = re.search(r'\.info_text\s*=\s*("(?:\\.|[^"\\])*")', entry)
            self.assertIsNotNone(match, name)
            text = ast.literal_eval(match.group(1))
            self.assertIn("DESCRIPTION\n", text, name)
            self.assertIn("\n\nGOAL\n", text, name)
            self.assertIn("\n\nCONTROLS\n", text, name)
            for line in text.split("\n"):
                self.assertLessEqual(len(line), 34, f"{name}: {line}")


if __name__ == "__main__":
    unittest.main()
