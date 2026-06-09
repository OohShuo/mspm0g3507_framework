#pragma once

// === ST7789 commands used by the driver (write-only) ===

#define ST7789_SWRESET      0x01
#define ST7789_SLPIN        0x10
#define ST7789_SLPOUT       0x11
#define ST7789_INVON        0x21
#define ST7789_DISPON       0x29
#define ST7789_CASET        0x2A
#define ST7789_RASET        0x2B
#define ST7789_RAMWR        0x2C
#define ST7789_MADCTL       0x36
#define ST7789_COLMOD       0x3A
#define ST7789_PORCTRL      0xB2
#define ST7789_GCTRL        0xB7
#define ST7789_VCOMS        0xBB
#define ST7789_LCMCTRL      0xC0
#define ST7789_VDVVRHEN     0xC2
#define ST7789_VRHS         0xC3
#define ST7789_VDVS         0xC4
#define ST7789_FRCTRL2      0xC6
#define ST7789_PWCTRL1      0xD0
#define ST7789_PVGAMCTRL    0xE0
#define ST7789_NVGAMCTRL    0xE1

// === MADCTL bits (0x36) ===

#define ST7789_MADCTL_BGR   0x08
#define ST7789_MADCTL_MX    0x40
#define ST7789_MADCTL_MY    0x80

// === COLMOD pixel format (0x3A, lower 3 bits) ===

#define ST7789_COLMOD_16BPP 0x55
#define ST7789_COLMOD_18BPP 0x66
