#include "game_info_text.h"

#include <stddef.h>

uint8_t Game_Info_Text_Next_Line(const char** cursor, char line[GAME_INFO_LINE_BUFFER_SIZE]) {
    if (cursor == NULL || *cursor == NULL || line == NULL) { return 0u; }

    const char* text = *cursor;
    uint8_t length = 0u;
    while (*text != '\0' && *text != '\n') {
        if (length < GAME_INFO_LINE_MAX) { line[length++] = *text; }
        text++;
    }
    line[length] = '\0';

    *cursor = *text == '\n' ? text + 1 : NULL;
    return 1u;
}
