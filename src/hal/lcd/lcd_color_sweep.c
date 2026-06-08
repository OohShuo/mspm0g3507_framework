#include "lcd_color_sweep.h"

/*============================================================================
 * Configuration
 *============================================================================*/

/**
 * SWEEP_STEP  – step size for the 8-bit channel value (0‑255).
 *
 *   step=1  → 256 frames/sweep  (~90 s,  Debug)
 *   step=4  →  64 frames/sweep  (~22 s,  Debug)
 *   step=8  →  32 frames/sweep  (~11 s,  Debug)
 *   step=8  →  32 frames/sweep  (~ 2 s,  Release)  ← default
 */
#define SWEEP_STEP  8

/**
 * RANDOM_WALK_STEPS – number of frames for the final random-walk phase.
 */
#define RANDOM_WALK_STEPS  256

/*============================================================================
 * Local helpers
 *============================================================================*/

/**
 * Convert 8‑bit R/G/B channels to the RGB565 16‑bit colour word.
 *
 * Layout:  R[4:0]  G[5:0]  B[4:0]    (bits 15‑11 / 10‑5 / 4‑0)
 */
static inline uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b)
{
    return (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

/**
 * Simple LCG pseudo‑random number generator (glibc-style).
 * Returns a value in the range [0, 0x7FFF].
 */
static uint32_t rand_lcg(void)
{
    static uint32_t seed = 12345;
    seed = seed * 1103515245UL + 12345UL;
    return (seed >> 16) & 0x7FFF;
}

/**
 * Fill the whole screen with one RGB565 colour.
 *
 * Uses the optimised St7789_Fill_Color which sets CASET+RASET once and
 * streams pixel data in one continuous CS-low burst.  Much faster than
 * calling St7789_Flush per row.
 */
static void fill_screen(St7789* lcd, uint16_t color)
{
    St7789_Fill_Color(lcd, color);
}

/*============================================================================
 * Sweep phases
 *============================================================================*/

static void phase_single(St7789* lcd, uint8_t r_mask,
                         uint8_t g_mask, uint8_t b_mask)
{
    uint16_t v;
    for (v = 0; v + SWEEP_STEP <= 255; v += SWEEP_STEP) {
        fill_screen(lcd, rgb565(r_mask ? (uint8_t)v : 0,
                                g_mask ? (uint8_t)v : 0,
                                b_mask ? (uint8_t)v : 0));
    }
    /* Always hit the endpoint (255) for the active channel(s) */
    fill_screen(lcd, rgb565(r_mask ? 255 : 0,
                            g_mask ? 255 : 0,
                            b_mask ? 255 : 0));
}

static void phase_dual(St7789* lcd, uint8_t r_mask,
                       uint8_t g_mask, uint8_t b_mask)
{
    uint16_t v;
    for (v = 0; v + SWEEP_STEP <= 255; v += SWEEP_STEP) {
        fill_screen(lcd, rgb565(r_mask ? (uint8_t)v : 0,
                                g_mask ? (uint8_t)v : 0,
                                b_mask ? (uint8_t)v : 0));
    }
    fill_screen(lcd, rgb565(r_mask ? 255 : 0,
                            g_mask ? 255 : 0,
                            b_mask ? 255 : 0));
}

static void phase_rgb(St7789* lcd)
{
    uint16_t v;
    for (v = 0; v + SWEEP_STEP <= 255; v += SWEEP_STEP) {
        fill_screen(lcd, rgb565((uint8_t)v, (uint8_t)v, (uint8_t)v));
    }
    fill_screen(lcd, rgb565(255, 255, 255));
}

static void phase_random_walk(St7789* lcd)
{
    int r = 128, g = 128, b = 128;

    for (uint32_t step = 0; step < RANDOM_WALK_STEPS; step++) {
        /* Each channel changes by -7 .. +7 */
        r += (int)(rand_lcg() % 15) - 7;
        g += (int)(rand_lcg() % 15) - 7;
        b += (int)(rand_lcg() % 15) - 7;

        /* Clamp to [0, 255] */
        if (r < 0)   r = 0;
        if (r > 255) r = 255;
        if (g < 0)   g = 0;
        if (g > 255) g = 255;
        if (b < 0)   b = 0;
        if (b > 255) b = 255;

        fill_screen(lcd, rgb565((uint8_t)r, (uint8_t)g, (uint8_t)b));
    }
}

/*============================================================================
 * Public API
 *============================================================================*/

void LCD_ColorSweep_Demo(St7789* lcd)
{
    /* ---- Phase 1: R ---- */
    phase_single(lcd, 1, 0, 0);

    /* ---- Phase 2: G ---- */
    phase_single(lcd, 0, 1, 0);

    /* ---- Phase 3: B ---- */
    phase_single(lcd, 0, 0, 1);

    /* ---- Phase 4: R+G (→ yellow) ---- */
    phase_dual(lcd, 1, 1, 0);

    /* ---- Phase 5: R+B (→ magenta) ---- */
    phase_dual(lcd, 1, 0, 1);

    /* ---- Phase 6: G+B (→ cyan) ---- */
    phase_dual(lcd, 0, 1, 1);

    /* ---- Phase 7: R+G+B (→ grayscale) ---- */
    phase_rgb(lcd);

    /* ---- Phase 8: RGB random walk ---- */
    phase_random_walk(lcd);
}
