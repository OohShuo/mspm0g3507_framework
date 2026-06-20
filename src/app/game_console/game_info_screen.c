#include "game_info_screen.h"

#include <string.h>

#include "game_graphics.h"
#include "game_info_text.h"

#define SCREEN_WIDTH 240
#define CARD_X       8
#define CARD_Y       38
#define CARD_W       (SCREEN_WIDTH - CARD_X * 2)
#define CARD_H       252

#define COLOR_CARD    0x0842u
#define COLOR_BORDER  0x4208u
#define COLOR_CYAN    0x07ffu
#define COLOR_TEXT    0xc618u

static uint8_t is_heading(const char* line) {
    return strcmp(line, "DESCRIPTION") == 0 || strcmp(line, "GOAL") == 0 ||
           strcmp(line, "CONTROLS") == 0;
}

void Game_Info_Screen_Draw(
    St7789* lcd, const char* name, const char* info_text, uint16_t fps) {
    if (lcd == NULL || name == NULL || info_text == NULL) { return; }

    Game_Graphics_Draw_Top_Bar(lcd, name);
    Game_Graphics_Clear_Game_Area(lcd);

    Game_Graphics_Fill_Rect(lcd, CARD_X, CARD_Y, CARD_W, CARD_H, COLOR_CARD);
    Game_Graphics_Fill_Rect(lcd, CARD_X, CARD_Y, CARD_W, 1, COLOR_CYAN);
    Game_Graphics_Fill_Rect(lcd, CARD_X, CARD_Y + CARD_H - 1, CARD_W, 1, COLOR_BORDER);
    Game_Graphics_Fill_Rect(lcd, CARD_X, CARD_Y, 1, CARD_H, COLOR_BORDER);
    Game_Graphics_Fill_Rect(lcd, CARD_X + CARD_W - 1, CARD_Y, 1, CARD_H, COLOR_BORDER);

    Game_Graphics_Draw_Text(lcd, 84, 45, "GAME GUIDE", 1, COLOR_CYAN);
    Game_Graphics_Fill_Rect(lcd, 16, 57, SCREEN_WIDTH - 32, 1, COLOR_BORDER);

    const char* cursor = info_text;
    char line[GAME_INFO_LINE_BUFFER_SIZE];
    int32_t y = 66;
    while (y <= 280 && Game_Info_Text_Next_Line(&cursor, line)) {
        if (is_heading(line)) {
            Game_Graphics_Fill_Rect(lcd, 15, y, 3, 7, COLOR_CYAN);
            Game_Graphics_Draw_Text(lcd, 23, y, line, 1, COLOR_CYAN);
        } else if (line[0] != '\0') {
            Game_Graphics_Draw_Text(lcd, 16, y, line, 1, COLOR_TEXT);
        }
        y += 12;
    }

    Game_Graphics_Draw_Bottom_Bar(lcd, "B BACK", fps);
}
