#include "game_control_hint.h"

#include <stddef.h>

static void append_text(char** cursor, uint8_t* remaining, const char* text) {
    while (*text != '\0' && *remaining > 1u) {
        **cursor = *text;
        (*cursor)++;
        text++;
        (*remaining)--;
    }
    **cursor = '\0';
}

void Game_Control_Hint_Format(
    const char* controls, uint8_t is_game, char output[GAME_CONTROL_HINT_TEXT_MAX]) {
    char* cursor = output;
    uint8_t remaining = GAME_CONTROL_HINT_TEXT_MAX;
    output[0] = '\0';

    if (!is_game) {
        append_text(&cursor, &remaining, "A OK  B BACK");
        return;
    }
    if (controls != NULL && controls[0] != '\0') {
        append_text(&cursor, &remaining, controls);
        append_text(&cursor, &remaining, "  ");
    }
    append_text(&cursor, &remaining, "X/B PAUSE");
}
