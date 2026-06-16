#include "game_over_menu.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "game_graphics.h"
#include "score_store.h"

#define SCREEN_WIDTH  240
#define SCREEN_HEIGHT 320

#define KEY_COLUMNS   6u
#define KEY_ROWS      6u
#define KEY_COUNT     36u
#define KEY_DELETE    36u
#define KEY_SAVE      37u

#define COLOR_BLACK   0x0000u
#define COLOR_WHITE   0xffffu
#define COLOR_CYAN    0x07ffu
#define COLOR_BLUE    0x001fu
#define COLOR_GREEN   0x07e0u
#define COLOR_YELLOW  0xffe0u
#define COLOR_RED     0xf800u
#define COLOR_GRAY    0x8410u
#define COLOR_DARK    0x2104u

typedef enum {
    end_stage_prompt,
    end_stage_keyboard,
    end_stage_leaderboard,
} End_stage;

static const char g_keyboard_characters[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

static St7789* g_lcd = NULL;
static Buzzer* g_buzzer = NULL;
static const char* g_game_name = NULL;
static End_stage g_stage = end_stage_prompt;
static uint32_t g_score = 0;
static uint8_t g_game_id = 0;
static uint8_t g_score_qualifies = 0;
static uint8_t g_prompt_selection = 0;
static uint8_t g_keyboard_selection = 0;
static uint8_t g_board_selection = 0;
static uint8_t g_name_length = 0;
static uint8_t g_new_rank = 0xffu;
static char g_player_name[SCORE_STORE_NAME_LENGTH + 1u];

static int32_t text_width(const char* text, uint8_t scale) {
    return text == NULL ? 0 : (int32_t)strlen(text) * 6 * scale;
}

static void draw_centered(int32_t y, const char* text, uint8_t scale, uint16_t color) {
    const int32_t x = (SCREEN_WIDTH - text_width(text, scale)) / 2;
    Game_Graphics_Draw_Text(g_lcd, x, y, text, scale, color);
}

static void draw_prompt_button(uint8_t selection, uint8_t selected) {
    const int32_t x = selection == 0 ? 18 : 126;
    const int32_t width = 96;
    const uint16_t border =
        selected ? COLOR_CYAN : (selection == 0 && !g_score_qualifies ? COLOR_DARK : COLOR_GRAY);
    const uint16_t background = selected ? COLOR_BLUE : COLOR_BLACK;

    Game_Graphics_Fill_Rect(g_lcd, x, 205, width, 45, border);
    Game_Graphics_Fill_Rect(g_lcd, x + 3, 208, width - 6, 39, background);
    if (selection == 0) {
        Game_Graphics_Draw_Text(
            g_lcd, x + 15, 218, "ENTER NAME", 1, g_score_qualifies ? COLOR_WHITE : COLOR_GRAY);
    } else {
        Game_Graphics_Draw_Text(g_lcd, x + 34, 218, "SKIP", 1, COLOR_WHITE);
    }
}

static void render_prompt(void) {
    Game_Graphics_Fill_Rect(g_lcd, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, COLOR_BLACK);
    draw_centered(18, "GAME RESULT", 2, COLOR_RED);
    draw_centered(53, g_game_name, 2, COLOR_CYAN);
    draw_centered(93, "YOUR SCORE", 1, COLOR_WHITE);
    Game_Graphics_Draw_U32(g_lcd, 84, 114, g_score, 6, 2, COLOR_YELLOW);

    draw_centered(164, g_score_qualifies ? "TOP 10 SCORE" : "TOP 10 FULL", 1,
        g_score_qualifies ? COLOR_GREEN : COLOR_GRAY);
    draw_prompt_button(0, g_prompt_selection == 0);
    draw_prompt_button(1, g_prompt_selection == 1);
    draw_centered(279, "CHOOSE WITH JOYSTICK", 1, COLOR_WHITE);
    draw_centered(298, "PRESS TO CONFIRM", 1, COLOR_GRAY);
}

static void draw_name(void) {
    Game_Graphics_Fill_Rect(g_lcd, 38, 58, 164, 30, COLOR_BLACK);
    for (uint8_t i = 0; i < SCORE_STORE_NAME_LENGTH; i++) {
        const int32_t x = 43 + i * 27;
        Game_Graphics_Fill_Rect(g_lcd, x, 82, 20, 2, i == g_name_length ? COLOR_CYAN : COLOR_GRAY);
        if (i < g_name_length) {
            char text[2] = {g_player_name[i], '\0'};
            Game_Graphics_Draw_Text(g_lcd, x + 7, 65, text, 1, COLOR_YELLOW);
        }
    }
}

static void draw_character_key(uint8_t index, uint8_t selected) {
    const uint8_t row = index / KEY_COLUMNS;
    const uint8_t column = index % KEY_COLUMNS;
    const int32_t x = 10 + column * 37;
    const int32_t y = 96 + row * 24;
    const uint16_t border = selected ? COLOR_CYAN : COLOR_DARK;
    const uint16_t background = selected ? COLOR_BLUE : COLOR_BLACK;
    char text[2] = {g_keyboard_characters[index], '\0'};

    Game_Graphics_Fill_Rect(g_lcd, x, y, 34, 21, border);
    Game_Graphics_Fill_Rect(g_lcd, x + 2, y + 2, 30, 17, background);
    Game_Graphics_Draw_Text(g_lcd, x + 14, y + 7, text, 1, COLOR_WHITE);
}

static void draw_special_key(uint8_t index, uint8_t selected) {
    const int32_t x = index == KEY_DELETE ? 27 : 128;
    const uint16_t border = selected ? COLOR_CYAN : COLOR_DARK;
    const uint16_t background = selected ? COLOR_BLUE : COLOR_BLACK;
    const char* label = index == KEY_DELETE ? "DEL" : "SAVE";

    Game_Graphics_Fill_Rect(g_lcd, x, 243, 84, 27, border);
    Game_Graphics_Fill_Rect(g_lcd, x + 2, 245, 80, 23, background);
    Game_Graphics_Draw_Text(g_lcd, x + (index == KEY_DELETE ? 33 : 30), 252, label, 1, COLOR_WHITE);
}

static void draw_keyboard_key(uint8_t index, uint8_t selected) {
    if (index < KEY_COUNT) {
        draw_character_key(index, selected);
    } else {
        draw_special_key(index, selected);
    }
}

static void render_keyboard(void) {
    Game_Graphics_Fill_Rect(g_lcd, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, COLOR_BLACK);
    draw_centered(10, "ENTER NAME", 2, COLOR_CYAN);
    Game_Graphics_Draw_Text(g_lcd, 19, 39, "SCORE", 1, COLOR_WHITE);
    Game_Graphics_Draw_U32(g_lcd, 61, 39, g_score, 6, 1, COLOR_YELLOW);
    draw_name();

    for (uint8_t index = 0; index < KEY_COUNT; index++) {
        draw_character_key(index, g_keyboard_selection == index);
    }
    draw_special_key(KEY_DELETE, g_keyboard_selection == KEY_DELETE);
    draw_special_key(KEY_SAVE, g_keyboard_selection == KEY_SAVE);
    draw_centered(286, "MOVE AND PRESS", 1, COLOR_GRAY);
}

static void draw_board_button(uint8_t selection, uint8_t selected) {
    const int32_t x = selection == 0 ? 17 : 130;
    const int32_t width = 93;
    const uint16_t border = selected ? COLOR_CYAN : COLOR_DARK;
    Game_Graphics_Fill_Rect(g_lcd, x, 281, width, 29, border);
    Game_Graphics_Fill_Rect(g_lcd, x + 2, 283, width - 4, 25, selected ? COLOR_BLUE : COLOR_BLACK);
    Game_Graphics_Draw_Text(
        g_lcd, x + (selection == 0 ? 28 : 34), 291, selection == 0 ? "REPLAY" : "MENU", 1, COLOR_WHITE);
}

static void render_leaderboard(void) {
    Game_Graphics_Fill_Rect(g_lcd, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, COLOR_BLACK);
    draw_centered(7, "TOP 10", 2, COLOR_CYAN);
    draw_centered(29, g_game_name, 1, COLOR_YELLOW);
    Game_Graphics_Draw_Text(g_lcd, 10, 48, "NO", 1, COLOR_GRAY);
    Game_Graphics_Draw_Text(g_lcd, 43, 48, "NAME", 1, COLOR_GRAY);
    Game_Graphics_Draw_Text(g_lcd, 139, 48, "SCORE", 1, COLOR_GRAY);
    Game_Graphics_Fill_Rect(g_lcd, 8, 59, 224, 1, COLOR_DARK);

    const uint8_t count = Score_Store_Get_Count(g_game_id);
    for (uint8_t rank = 0; rank < SCORE_STORE_TOP_COUNT; rank++) {
        const int32_t y = 66 + rank * 20;
        const uint16_t color = rank == g_new_rank ? COLOR_CYAN : COLOR_WHITE;
        const Score_entry* entry = rank < count ? Score_Store_Get_Entry(g_game_id, rank) : NULL;
        Game_Graphics_Draw_U32(g_lcd, 10, y, rank + 1u, 2, 1, color);
        if (entry != NULL) {
            Game_Graphics_Draw_Text(g_lcd, 43, y, entry->name, 1, color);
            Game_Graphics_Draw_U32(g_lcd, 139, y, entry->score, 6, 1, color);
        } else {
            Game_Graphics_Draw_Text(g_lcd, 43, y, "------", 1, COLOR_DARK);
            Game_Graphics_Draw_Text(g_lcd, 139, y, "------", 1, COLOR_DARK);
        }
    }

    draw_board_button(0, g_board_selection == 0);
    draw_board_button(1, g_board_selection == 1);
}

static uint8_t move_keyboard_selection(uint8_t selection, Game_direction direction) {
    if (selection < KEY_COUNT) {
        const uint8_t row = selection / KEY_COLUMNS;
        const uint8_t column = selection % KEY_COLUMNS;
        if (direction == game_direction_left) {
            return row * KEY_COLUMNS + (column == 0 ? KEY_COLUMNS - 1u : column - 1u);
        }
        if (direction == game_direction_right) { return row * KEY_COLUMNS + (column + 1u) % KEY_COLUMNS; }
        if (direction == game_direction_up) {
            return row == 0 ? (column < 3u ? KEY_DELETE : KEY_SAVE) : (uint8_t)(selection - KEY_COLUMNS);
        }
        if (direction == game_direction_down) {
            return row == KEY_ROWS - 1u ? (column < 3u ? KEY_DELETE : KEY_SAVE)
                                        : (uint8_t)(selection + KEY_COLUMNS);
        }
        return selection;
    }

    if (direction == game_direction_left || direction == game_direction_right) {
        return selection == KEY_DELETE ? KEY_SAVE : KEY_DELETE;
    }
    if (direction == game_direction_up) { return selection == KEY_DELETE ? 31u : 34u; }
    if (direction == game_direction_down) { return selection == KEY_DELETE ? 1u : 4u; }
    return selection;
}

static Game_over_action update_prompt(const Game_input* input) {
    if (input->back_requested) { return game_over_action_menu; }
    if (input->direction_pressed) {
        uint8_t next = g_prompt_selection;
        if (input->direction == game_direction_left || input->direction == game_direction_up) {
            next = 0;
        } else if (input->direction == game_direction_right || input->direction == game_direction_down) {
            next = 1;
        }
        if (!g_score_qualifies) { next = 1; }
        if (next != g_prompt_selection) {
            const uint8_t old = g_prompt_selection;
            g_prompt_selection = next;
            draw_prompt_button(old, 0);
            draw_prompt_button(g_prompt_selection, 1);
            Buzzer_Play_Sfx(g_buzzer, buzzer_sfx_menu_move);
        }
    }

    if (!input->confirm_pressed) { return game_over_action_none; }
    Buzzer_Play_Sfx(g_buzzer, buzzer_sfx_menu_select);
    if (g_prompt_selection == 0 && g_score_qualifies) {
        g_stage = end_stage_keyboard;
        g_keyboard_selection = 0;
        g_name_length = 0;
        memset(g_player_name, 0, sizeof(g_player_name));
        render_keyboard();
    } else {
        g_stage = end_stage_leaderboard;
        g_new_rank = 0xffu;
        render_leaderboard();
    }
    return game_over_action_none;
}

static Game_over_action update_keyboard(const Game_input* input) {
    if (input->back_requested) {
        g_stage = end_stage_prompt;
        render_prompt();
        return game_over_action_none;
    }

    if (input->direction_pressed) {
        const uint8_t old = g_keyboard_selection;
        g_keyboard_selection = move_keyboard_selection(g_keyboard_selection, input->direction);
        if (old != g_keyboard_selection) {
            draw_keyboard_key(old, 0);
            draw_keyboard_key(g_keyboard_selection, 1);
            Buzzer_Play_Sfx(g_buzzer, buzzer_sfx_menu_move);
        }
    }

    if (!input->confirm_pressed) { return game_over_action_none; }
    if (g_keyboard_selection < KEY_COUNT) {
        if (g_name_length < SCORE_STORE_NAME_LENGTH) {
            g_player_name[g_name_length++] = g_keyboard_characters[g_keyboard_selection];
            g_player_name[g_name_length] = '\0';
            draw_name();
            Buzzer_Play_Sfx(g_buzzer, buzzer_sfx_menu_select);
        }
    } else if (g_keyboard_selection == KEY_DELETE) {
        if (g_name_length > 0) {
            g_player_name[--g_name_length] = '\0';
            draw_name();
            Buzzer_Play_Sfx(g_buzzer, buzzer_sfx_menu_move);
        }
    } else if (g_name_length > 0) {
        g_new_rank = Score_Store_Add(g_game_id, g_player_name, g_score);
        Score_Store_Commit();
        g_stage = end_stage_leaderboard;
        g_board_selection = 0;
        Buzzer_Play_Sfx(g_buzzer, buzzer_sfx_menu_select);
        render_leaderboard();
    }
    return game_over_action_none;
}

static Game_over_action update_leaderboard(const Game_input* input) {
    if (input->back_requested) { return game_over_action_menu; }
    if (input->direction_pressed &&
        (input->direction == game_direction_left || input->direction == game_direction_right ||
            input->direction == game_direction_up || input->direction == game_direction_down)) {
        const uint8_t old = g_board_selection;
        g_board_selection ^= 1u;
        draw_board_button(old, 0);
        draw_board_button(g_board_selection, 1);
        Buzzer_Play_Sfx(g_buzzer, buzzer_sfx_menu_move);
    }
    if (!input->confirm_pressed) { return game_over_action_none; }
    Buzzer_Play_Sfx(g_buzzer, buzzer_sfx_menu_select);
    return g_board_selection == 0 ? game_over_action_replay : game_over_action_menu;
}

void Game_Over_Menu_Open(
    St7789* lcd, Buzzer* buzzer, uint8_t game_id, const char* game_name, uint32_t score) {
    g_lcd = lcd;
    g_buzzer = buzzer;
    g_game_id = game_id;
    g_game_name = game_name;
    g_score = score;
    g_score_qualifies = Score_Store_Qualifies(game_id, score);
    g_prompt_selection = g_score_qualifies ? 0 : 1;
    g_board_selection = 0;
    g_new_rank = 0xffu;
    g_stage = end_stage_prompt;
    render_prompt();
}

Game_over_action Game_Over_Menu_Update(const Game_input* input) {
    if (input == NULL || g_lcd == NULL) { return game_over_action_none; }
    if (g_stage == end_stage_prompt) { return update_prompt(input); }
    if (g_stage == end_stage_keyboard) { return update_keyboard(input); }
    return update_leaderboard(input);
}
