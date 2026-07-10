import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[2]
GAMES = ROOT / "src/app/games"


class GameContractTest(unittest.TestCase):
    def test_finish_polling_is_removed(self):
        text = "\n".join(p.read_text() for p in (ROOT / "src/app").rglob("*.[ch]"))
        self.assertNotRegex(text, r"\b[A-Za-z0-9_]+_Is_Finished\b|\.is_finished\b")

    def test_games_do_not_own_generic_terminal_feedback(self):
        sources = [p for p in GAMES.rglob("*.c") if p.name != "pacman.c"]
        text = "\n".join(p.read_text() for p in sources)
        self.assertNotRegex(text, r"buzzer_sfx_(victory|defeat)|vib_effect_(victory|defeat)")
        pacman = (GAMES / "pacman/pacman.c").read_text()
        self.assertNotRegex(pacman, r"buzzer_sfx_defeat|vib_effect_defeat")

    def test_games_do_not_offer_terminal_restart_prompts(self):
        text = "\n".join(p.read_text() for p in GAMES.rglob("*.c"))
        self.assertNotRegex(text, r'"[^"\n]*RESTART[^"\n]*"')

    def test_tools_never_report_game_outcomes(self):
        tools = ["bad_apple", "calculator", "fps_test", "info", "sfx_lib", "volume_control"]
        for tool in tools:
            text = (GAMES / tool / f"{tool}.c").read_text()
            self.assertNotIn("game_result_won", text)
            self.assertNotIn("game_result_lost", text)


if __name__ == "__main__":
    unittest.main()
