#include "volume_control.h"

#include <stddef.h>

#include "game_graphics.h"

#define SCREEN_WIDTH  240
#define SCREEN_HEIGHT 320

#define COLOR_BLACK   0x0000u
#define COLOR_WHITE   0xffffu
#define COLOR_CYAN    0x07ffu
#define COLOR_GRAY    0x8410u
#define COLOR_DARK    0x4208u
#define COLOR_GREEN   0x07e0u

#define BAR_X         20
#define BAR_Y         153
#define BAR_W         200
#define BAR_H         24
#define BAR_BORDER    2

/* Fillable area inside the bar border */
#define BAR_FILL_X    (BAR_X + BAR_BORDER)
#define BAR_FILL_W    (BAR_W - BAR_BORDER * 2)

/* Percentage text area in top bar (scale 1) */
#define TEXT_X        170
#define TEXT_Y        6
#define TEXT_W        65
#define TEXT_H        10

/* Arrow bounding boxes */
#define ARROW_UP_X    116
#define ARROW_UP_Y    119
#define ARROW_UP_W    13
#define ARROW_UP_H    15

#define ARROW_DN_X    116
#define ARROW_DN_Y    202
#define ARROW_DN_W    13
#define ARROW_DN_H    9

static St7789* g_lcd = NULL;
static Buzzer* g_buzzer = NULL;
static Vib_motor* g_vib_motor = NULL;
static uint8_t g_volume = 50;
static uint8_t g_muted = 0;
static uint8_t g_pre_mute_volume = 50;

/* ── Draw bar outline and background (once, never redrawn) ── */
static void draw_bar_frame(void) { Game_Graphics_Fill_Rect(g_lcd, BAR_X, BAR_Y, BAR_W, BAR_H, COLOR_DARK); }

/* ── Draw one arrow (up or down) ── */
static void draw_up_arrow(void) {
    Game_Graphics_Fill_Rect(g_lcd, BAR_X + BAR_W / 2 - 4, BAR_Y - 25, 9, 3, COLOR_CYAN);
    Game_Graphics_Fill_Rect(g_lcd, BAR_X + BAR_W / 2 - 2, BAR_Y - 28, 5, 3, COLOR_CYAN);
    Game_Graphics_Fill_Rect(g_lcd, BAR_X + BAR_W / 2, BAR_Y - 31, 1, 3, COLOR_CYAN);
}

static void draw_down_arrow(void) {
    Game_Graphics_Fill_Rect(g_lcd, BAR_X + BAR_W / 2 - 4, BAR_Y + BAR_H + 25, 9, 3, COLOR_CYAN);
    Game_Graphics_Fill_Rect(g_lcd, BAR_X + BAR_W / 2 - 2, BAR_Y + BAR_H + 28, 5, 3, COLOR_CYAN);
    Game_Graphics_Fill_Rect(g_lcd, BAR_X + BAR_W / 2, BAR_Y + BAR_H + 31, 1, 3, COLOR_CYAN);
}

/* ── Incremental bar fill update ── */
static void update_bar_fill(uint8_t old_vol, uint8_t new_vol) {
    const uint8_t old_fw = (uint8_t)((uint16_t)BAR_FILL_W * old_vol / 100u);
    const uint8_t new_fw = (uint8_t)((uint16_t)BAR_FILL_W * new_vol / 100u);

    if (new_fw > old_fw) {
        /* Volume increased — draw new filled portion */
        Game_Graphics_Fill_Rect(g_lcd, BAR_FILL_X + old_fw, BAR_Y + BAR_BORDER, new_fw - old_fw,
            BAR_H - BAR_BORDER * 2, COLOR_GREEN);
    } else if (new_fw < old_fw) {
        /* Volume decreased — erase removed portion back to bar background */
        Game_Graphics_Fill_Rect(g_lcd, BAR_FILL_X + new_fw, BAR_Y + BAR_BORDER, old_fw - new_fw,
            BAR_H - BAR_BORDER * 2, COLOR_DARK);
    }
}

/* ── Incremental text update ── */
static void update_text(uint8_t old_vol, uint8_t new_vol) {
    (void)old_vol;
    /* Clear text area with bar color so it blends in top bar */
    Game_Graphics_Fill_Rect(g_lcd, TEXT_X, TEXT_Y, TEXT_W, TEXT_H, GAME_BAR_COLOR_BG);
    /* Draw new percentage right-aligned, scale 1 */
    const uint8_t digits = new_vol >= 100 ? 3 : (new_vol >= 10 ? 2 : 1);
    const int32_t text_w = (int32_t)(digits + 1) * 6; /* scale 1 */
    const int32_t text_x = TEXT_X + TEXT_W - text_w;  /* right-align */
    Game_Graphics_Draw_U32(g_lcd, text_x, TEXT_Y, new_vol, digits, 1, COLOR_WHITE);
    Game_Graphics_Draw_Text(g_lcd, text_x + (int32_t)digits * 6, TEXT_Y, "%", 1, COLOR_WHITE);
}

/* ── Incremental arrow update ── */
static void update_arrows(uint8_t old_vol, uint8_t new_vol) {
    const uint8_t old_up = old_vol < 100;
    const uint8_t new_up = new_vol < 100;
    const uint8_t old_dn = old_vol > 0;
    const uint8_t new_dn = new_vol > 0;

    if (old_up != new_up) {
        Game_Graphics_Fill_Rect(g_lcd, ARROW_UP_X, ARROW_UP_Y, ARROW_UP_W, ARROW_UP_H, COLOR_BLACK);
        if (new_up) { draw_up_arrow(); }
    }
    if (old_dn != new_dn) {
        Game_Graphics_Fill_Rect(g_lcd, ARROW_DN_X, ARROW_DN_Y, ARROW_DN_W, ARROW_DN_H, COLOR_BLACK);
        if (new_dn) { draw_down_arrow(); }
    }
}

/* ── Full screen render (called once on init) ── */
static void render_screen(void) {
    Game_Graphics_Clear_Game_Area(g_lcd);

    /* Volume text in top bar */
    update_text(0, g_volume);

    /* Bar frame */
    draw_bar_frame();

    /* Initial fill */
    update_bar_fill(0, g_volume);

    /* Arrows: up = can increase, down = can decrease */
    if (g_volume < 100) { draw_up_arrow(); }
    if (g_volume > 0) { draw_down_arrow(); }
}

/* ── Game lifecycle ── */
void Volume_Control_Init(const Game_hardware* hardware) {
    g_lcd = hardware->lcd;
    g_buzzer = hardware->buzzer;
    g_vib_motor = hardware->vib_motor;
    g_volume = Buzzer_Get_Volume(g_buzzer);
    render_screen();
}

Game_result Volume_Control_Update(const Game_input* input) {
    if (input->back_requested) { return game_result_exit; }

    /* mute toggle on button press */
    if (input->confirm_pressed) {
        if (g_muted) {
            g_muted = 0;
            g_volume = g_pre_mute_volume;
        } else {
            g_muted = 1;
            g_pre_mute_volume = g_volume;
            g_volume = 0;
        }
        Buzzer_Set_Volume(g_buzzer, g_volume);
        Buzzer_Play_Sfx(g_buzzer, buzzer_sfx_menu_select);
        Vib_Motor_Play_Effect(g_vib_motor, vib_effect_menu_select);
        render_screen();
        return game_result_running;
    }

    if (input->direction_pressed) {
        const uint8_t old_volume = g_volume;

        /* up / right → increase, down / left → decrease */
        if ((input->direction == game_direction_up || input->direction == game_direction_right) &&
            g_volume < 100) {
            g_volume += 10;
        } else if ((input->direction == game_direction_down || input->direction == game_direction_left) &&
                   g_volume > 0) {
            g_volume -= 10;
        }

        if (g_volume != old_volume) {
            g_muted = 0;
            Buzzer_Set_Volume(g_buzzer, g_volume);
            Buzzer_Play_Sfx(g_buzzer, buzzer_sfx_menu_select);
            Vib_Motor_Play_Effect(g_vib_motor, vib_effect_menu_tick);
            update_bar_fill(old_volume, g_volume);
            update_text(old_volume, g_volume);
            update_arrows(old_volume, g_volume);
        }
    }

    return game_result_running;
}

uint32_t Volume_Control_Get_Score(void) { return 0; }

uint8_t Volume_Control_Is_Finished(void) { return 0; }
