#ifndef LCD_COLOR_SWEEP_H
#define LCD_COLOR_SWEEP_H

#include "st7789.h"

/**
 * @brief   Run a full-screen color sweep demo on the ST7789 display.
 *
 * Cycles through 8 phases using St7789_Flush:
 *   1. R  only   (0 → 255)
 *   2. G  only   (0 → 255)
 *   3. B  only   (0 → 255)
 *   4. R + G     (yellow sweep)
 *   5. R + B     (magenta sweep)
 *   6. G + B     (cyan sweep)
 *   7. R + G + B (grayscale sweep)
 *   8. RGB random walk
 *
 * @param lcd  An initialised ST7789 instance.
 */
void LCD_ColorSweep_Demo(St7789* lcd);

#endif /* LCD_COLOR_SWEEP_H */
