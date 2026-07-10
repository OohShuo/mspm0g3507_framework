#include <string.h>

#include "game_graphics.h"
#include "game_registry.h"

/* ── Layout ── */
#define SCREEN_WIDTH  240
#define SCREEN_HEIGHT 320

#define BTN_W         50
#define BTN_H         36
#define GAP_X         8
#define GAP_Y         8
#define GRID_X0       8
#define GRID_Y0       59
#define ROWS          5
#define COLS          4

#define MAX_EXPR      64

/* ── Colors ── */
#define COLOR_BLACK   0x0000u
#define COLOR_WHITE   0xffffu
#define COLOR_CYAN    0x07ffu
#define COLOR_BLUE    0x053fu
#define COLOR_GRAY    0x8410u
#define COLOR_DARK    0xA514u
#define COLOR_RED     0xf800u
#define COLOR_GREEN   0x07e0u
#define COLOR_ORANGE  0xfc00u

/* ── Button type for coloring ── */
typedef enum {
    btn_num,
    btn_op,
    btn_ac,
    btn_del,
    btn_eq,
} Btn_type;

/* ── Static state ── */
static St7789* g_lcd = NULL;
static Vib_motor_gpio* g_vib_motor = NULL;
static char g_expr[MAX_EXPR];
static uint8_t g_expr_len = 0;
static uint8_t g_cursor_row = 2;
static uint8_t g_cursor_col = 1;
static uint8_t g_just_evaluated = 0;
static float g_last_result = 0.0f;
static uint8_t g_error = 0;

/* ── Button labels ── */
static const char* g_buttons[ROWS][COLS] = {
    {"AC", "DEL", "/", "*"},
    {"7", "8", "9", "-"},
    {"4", "5", "6", "+"},
    {"1", "2", "3", "="},
    {"0", ".", "(", ")"},
};

/* ── Helpers ── */
static int32_t btn_x(uint8_t col) { return GRID_X0 + (int32_t)col * (BTN_W + GAP_X); }
static int32_t btn_y(uint8_t row) { return GRID_Y0 + (int32_t)row * (BTN_H + GAP_Y); }

static Btn_type btn_type(uint8_t row, uint8_t col) {
    if (row == 0 && col == 0) return btn_ac;
    if (row == 0 && col == 1) return btn_del;
    if (row == 3 && col == 3) return btn_eq;
    if (col == 3 && !(row == 4)) return btn_op;
    return btn_num;
}

/* ── Draw single button ── */
static void draw_button(uint8_t row, uint8_t col, uint8_t selected) {
    const int32_t x = btn_x(col);
    const int32_t y = btn_y(row);
    const char* label = g_buttons[row][col];
    const Btn_type bt = btn_type(row, col);

    uint16_t bg;
    switch (bt) {
        case btn_ac:
            bg = selected ? COLOR_RED : 0x2104u;
            break;
        case btn_del:
            bg = selected ? COLOR_ORANGE : 0x2104u;
            break;
        case btn_eq:
            bg = selected ? COLOR_GREEN : 0x2104u;
            break;
        case btn_op:
            bg = selected ? COLOR_CYAN : 0x2104u;
            break;
        default:
            bg = selected ? COLOR_BLUE : 0x2104u;
            break;
    }

    const uint16_t border = selected ? COLOR_WHITE : COLOR_DARK;

    Game_Graphics_Fill_Rect(g_lcd, x, y, BTN_W, BTN_H, bg);
    Game_Graphics_Fill_Rect(g_lcd, x, y, BTN_W, 1, border);
    Game_Graphics_Fill_Rect(g_lcd, x, y + BTN_H - 1, BTN_W, 1, border);
    Game_Graphics_Fill_Rect(g_lcd, x, y, 1, BTN_H, border);
    Game_Graphics_Fill_Rect(g_lcd, x + BTN_W - 1, y, 1, BTN_H, border);

    const int32_t label_w = (int32_t)strlen(label) * 12;
    const int32_t text_x = x + (BTN_W - label_w) / 2;
    const int32_t text_y = y + (BTN_H - 16) / 2;
    const uint16_t text_color = (bt == btn_op && selected) ? COLOR_BLACK : COLOR_WHITE;
    Game_Graphics_Draw_Text(g_lcd, text_x, text_y, label, 2, text_color);
}

/* ── Redraw only two buttons ── */
static void redraw_button(uint8_t row, uint8_t col, uint8_t selected) { draw_button(row, col, selected); }

/* ── float → string helper (manual, avoids printf bloat) ── */
static uint8_t float_to_str(float val, char* buf, uint8_t max_len) {
    uint8_t pos = 0;
    if (val < 0.0f) {
        buf[pos++] = '-';
        val = -val;
    }

    int32_t int_part = (int32_t)val;
    float frac = val - (float)int_part;

    char rev[16];
    uint8_t ri = 0;
    if (int_part == 0) {
        rev[ri++] = '0';
    } else {
        while (int_part > 0 && ri < sizeof(rev)) {
            rev[ri++] = '0' + (char)(int_part % 10);
            int_part /= 10;
        }
    }
    while (ri > 0 && pos < max_len) buf[pos++] = rev[--ri];

    if (frac > 1e-6f && pos < max_len - 1) {
        buf[pos++] = '.';
        for (uint8_t i = 0; i < 6 && pos < max_len - 1; i++) {
            frac *= 10.0f;
            int d = (int)frac;
            buf[pos++] = '0' + (char)d;
            frac -= (float)d;
        }
        while (pos > 0 && buf[pos - 1] == '0') pos--;
        if (pos > 0 && buf[pos - 1] == '.') pos--;
    }
    buf[pos] = '\0';
    return pos;
}

/* ── Full display render ── */
static void render_display(void) {
    /* Expression in top bar right side ("CALC" ends ~80px, 5px margin) */
    Game_Graphics_Fill_Rect(g_lcd, 77, 4, 161, 22, GAME_BAR_COLOR_BG);
    if (g_expr_len > 0) {
        const char* show = g_expr;
        if (g_expr_len > 26) { show = g_expr + g_expr_len - 26; }
        Game_Graphics_Draw_Text(g_lcd, 84, 6, show, 1, COLOR_CYAN);
    }

    /* Error / result in game area */
    Game_Graphics_Fill_Rect(g_lcd, 10, GAME_AREA_Y + 2, SCREEN_WIDTH - 20, 18, COLOR_BLACK);
    if (g_error) {
        Game_Graphics_Draw_Text(g_lcd, 10, GAME_AREA_Y + 4, "Error", 2, COLOR_RED);
    } else if (g_just_evaluated) {
        char result_str[32];
        float_to_str(g_last_result, result_str, sizeof(result_str));
        Game_Graphics_Draw_Text(g_lcd, 10, GAME_AREA_Y + 4, "= ", 2, COLOR_GREEN);
        Game_Graphics_Draw_Text(g_lcd, 34, GAME_AREA_Y + 4, result_str, 2, COLOR_WHITE);
    }
}

static void render_grid(void) {
    for (uint8_t r = 0; r < ROWS; r++) {
        for (uint8_t c = 0; c < COLS; c++) { draw_button(r, c, r == g_cursor_row && c == g_cursor_col); }
    }
}

static void render_all(void) {
    Game_Graphics_Clear_Game_Area(g_lcd);
    render_display();
    render_grid();
}

/* ── Recursive-descent expression evaluator (all float) ── */
static uint8_t eval_error;

static float parse_expr(const char** p);
static float parse_term(const char** p);
static float parse_factor(const char** p);

static void skip_spaces(const char** p) {
    while (**p == ' ') (*p)++;
}

static float parse_number(const char** p) {
    float result = 0.0f;
    while (**p >= '0' && **p <= '9') {
        result = result * 10.0f + (float)(**p - '0');
        (*p)++;
    }
    if (**p == '.') {
        (*p)++;
        float frac = 0.1f;
        while (**p >= '0' && **p <= '9') {
            result += (float)(**p - '0') * frac;
            frac *= 0.1f;
            (*p)++;
        }
    }
    return result;
}

static float parse_factor(const char** p) {
    skip_spaces(p);
    if (**p == '(') {
        (*p)++;
        float val = parse_expr(p);
        skip_spaces(p);
        if (**p == ')') {
            (*p)++;
        } else {
            eval_error = 1;
        }
        return val;
    }
    if (**p == '-') {
        (*p)++;
        return -parse_factor(p);
    }
    return parse_number(p);
}

static float parse_term(const char** p) {
    float lhs = parse_factor(p);
    skip_spaces(p);
    while (**p == '*' || **p == '/') {
        char op = **p;
        (*p)++;
        float rhs = parse_factor(p);
        if (eval_error) return 0.0f;
        if (op == '*') {
            lhs *= rhs;
        } else {
            if (rhs == 0.0f) {
                eval_error = 1;
                return 0.0f;
            }
            lhs /= rhs;
        }
        skip_spaces(p);
    }
    return lhs;
}

static float parse_expr(const char** p) {
    float lhs = parse_term(p);
    skip_spaces(p);
    while (**p == '+' || **p == '-') {
        char op = **p;
        (*p)++;
        float rhs = parse_term(p);
        if (eval_error) return 0.0f;
        if (op == '+')
            lhs += rhs;
        else
            lhs -= rhs;
        skip_spaces(p);
    }
    return lhs;
}

static int evaluate(const char* s, float* out) {
    eval_error = 0;
    const char* p = s;
    float result = parse_expr(&p);
    skip_spaces(&p);
    if (*p != '\0') eval_error = 1;
    if (eval_error) return 0;
    *out = result;
    return 1;
}

/* ── Button press handler ── */
static uint8_t press_button(uint8_t row, uint8_t col) {
    const char* label = g_buttons[row][col];
    g_error = 0;

    if (label[0] == 'A' && label[1] == 'C') {
        g_expr_len = 0;
        g_expr[0] = '\0';
        g_just_evaluated = 0;
        render_display();
        return 1;
    }

    if (label[0] == 'D' && label[1] == 'E') {
        if (g_just_evaluated) {
            g_expr_len = 0;
            g_expr[0] = '\0';
            g_just_evaluated = 0;
        } else if (g_expr_len > 0) {
            g_expr[--g_expr_len] = '\0';
        }
        render_display();
        return 1;
    }

    if (label[0] == '=') {
        if (g_expr_len == 0) return 0;
        if (evaluate(g_expr, &g_last_result)) {
            g_just_evaluated = 1;
        } else {
            g_error = 1;
            g_just_evaluated = 0;
        }
        render_display();
        return 1;
    }

    if (g_just_evaluated) {
        if ((label[0] >= '0' && label[0] <= '9') || label[0] == '.' || label[0] == '(') {
            g_expr_len = 0;
            g_expr[0] = '\0';
        } else {
            g_expr_len = 0;
            char buf[32];
            uint8_t len = float_to_str(g_last_result, buf, sizeof(buf));
            for (uint8_t i = 0; i < len && g_expr_len < MAX_EXPR - 1; i++) { g_expr[g_expr_len++] = buf[i]; }
            g_expr[g_expr_len] = '\0';
        }
        g_just_evaluated = 0;
    }

    uint8_t label_len = (uint8_t)strlen(label);
    if (g_expr_len + label_len >= MAX_EXPR) return 0;
    for (uint8_t i = 0; i < label_len; i++) { g_expr[g_expr_len++] = label[i]; }
    g_expr[g_expr_len] = '\0';
    render_display();
    return 1;
}

/* ── Public API ── */
static void Calc_Init(const Game_hardware* hardware) {
    g_lcd = hardware->lcd;
    g_vib_motor = hardware->vib_motor;
    g_expr_len = 0;
    g_expr[0] = '\0';
    g_cursor_row = 2;
    g_cursor_col = 1;
    g_just_evaluated = 0;
    g_last_result = 0.0f;
    g_error = 0;
    render_all();
}

static Game_result Calc_Update(const Game_input* input) {
    if (input->back_requested) { return game_result_exit; }

    uint8_t cursor_changed = 0;

    if (input->direction_pressed) {
        const uint8_t old_row = g_cursor_row;
        const uint8_t old_col = g_cursor_col;

        switch (input->direction) {
            case game_direction_up:
                g_cursor_row = g_cursor_row == 0 ? ROWS - 1 : g_cursor_row - 1;
                break;
            case game_direction_down:
                g_cursor_row = g_cursor_row == ROWS - 1 ? 0 : g_cursor_row + 1;
                break;
            case game_direction_left:
                g_cursor_col = g_cursor_col == 0 ? COLS - 1 : g_cursor_col - 1;
                break;
            case game_direction_right:
                g_cursor_col = g_cursor_col == COLS - 1 ? 0 : g_cursor_col + 1;
                break;
            default:
                break;
        }

        if (g_cursor_row != old_row || g_cursor_col != old_col) {
            redraw_button(old_row, old_col, 0);
            redraw_button(g_cursor_row, g_cursor_col, 1);
            cursor_changed = 1;
        }
    }

    if (input->confirm_pressed && press_button(g_cursor_row, g_cursor_col)) {
        Vib_Motor_Gpio_Play_Effect(g_vib_motor, vib_effect_menu_select);
    } else if (cursor_changed) {
        Vib_Motor_Gpio_Play_Effect(g_vib_motor, vib_effect_menu_tick);
    }

    return game_result_running;
}

static uint32_t Calc_Get_Score(void) { return 0; }

static void calculator_draw_icon(St7789* lcd, int32_t x, int32_t y) {
    y += -2;
    /* Calculator body */
    Game_Graphics_Fill_Rect(lcd, x + 6, y + 4, 36, 30, 0xA514u);
    Game_Graphics_Fill_Rect(lcd, x + 8, y + 6, 32, 26, 0x0000u);

    /* Display bar */
    Game_Graphics_Fill_Rect(lcd, x + 10, y + 8, 28, 6, 0x2104u);

    /* Button grid: 4 cols × 3 rows */
    for (int32_t r = 0; r < 3; r++) {
        for (int32_t c = 0; c < 4; c++) {
            Game_Graphics_Fill_Rect(lcd, x + 10 + c * 7, y + 16 + r * 5, 5, 3, 0x3186u);
        }
    }

    /* Highlighted "=" button area */
    Game_Graphics_Fill_Rect(lcd, x + 24, y + 26, 5, 3, 0x07ffu);
}

const Game_descriptor game_calculator_entry = {
    .draw_icon = calculator_draw_icon,
    .name_color = 0x07ffu,
    .name = "CALC",
    .id = game_id_calculator,
    .control_hint = NULL,
    .info_text =
        "DESCRIPTION\nA compact four-function tool.\nEnter numbers and operators.\n\nGOAL\nCalculate a "
        "numeric result.\n\nCONTROLS\nJOY MOVE CURSOR\nA PRESS KEY\nB BACK",
    .is_game = 0,
    .init = Calc_Init,
    .update = Calc_Update,
    .get_score = Calc_Get_Score,
};
