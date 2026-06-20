#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "game_info_text.h"

int main(void) {
    const char* cursor = "ONE\n\nTWO";
    char line[GAME_INFO_LINE_BUFFER_SIZE];

    assert(Game_Info_Text_Next_Line(&cursor, line) == 1u);
    assert(strcmp(line, "ONE") == 0);
    assert(Game_Info_Text_Next_Line(&cursor, line) == 1u);
    assert(strcmp(line, "") == 0);
    assert(Game_Info_Text_Next_Line(&cursor, line) == 1u);
    assert(strcmp(line, "TWO") == 0);
    assert(Game_Info_Text_Next_Line(&cursor, line) == 0u);

    cursor = "12345678901234567890123456789012345\nEND";
    assert(Game_Info_Text_Next_Line(&cursor, line) == 1u);
    assert(strlen(line) == GAME_INFO_LINE_MAX);
    assert(Game_Info_Text_Next_Line(&cursor, line) == 1u);
    assert(strcmp(line, "END") == 0);
    return 0;
}
