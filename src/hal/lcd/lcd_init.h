#ifndef LCD_INIT_H
#define LCD_INIT_H

#include <stdint.h>
#include "board_config.h"
#include "dl_gpio.h"

#define USE_HORIZONTAL 0

#define LCD_W 240
#define LCD_H 240

#ifndef u8
#define u8 uint8_t
#endif

#ifndef u16
#define u16 uint16_t
#endif

#ifndef u32
#define u32 uint32_t
#endif

/*----------------- LCD pin control macros -----------------*/
/*
 * These are defined in board_config.h:
 *   LCD_SCLK_PORT / LCD_SCLK_PIN
 *   LCD_SDA_PORT  / LCD_SDA_PIN
 *   LCD_RES_PORT  / LCD_RES_PIN
 *   LCD_DC_PORT   / LCD_DC_PIN
 *   LCD_BLK_PORT  / LCD_BLK_PIN
 *
 * CS is tied to GND on this hardware.
 */

#define LCD_SCLK_Clr() DL_GPIO_clearPins(LCD_SCLK_PORT, LCD_SCLK_PIN)
#define LCD_SCLK_Set() DL_GPIO_setPins(LCD_SCLK_PORT, LCD_SCLK_PIN)

#define LCD_MOSI_Clr() DL_GPIO_clearPins(LCD_SDA_PORT, LCD_SDA_PIN)
#define LCD_MOSI_Set() DL_GPIO_setPins(LCD_SDA_PORT, LCD_SDA_PIN)

#define LCD_RES_Clr()  DL_GPIO_clearPins(LCD_RES_PORT, LCD_RES_PIN)
#define LCD_RES_Set()  DL_GPIO_setPins(LCD_RES_PORT, LCD_RES_PIN)

#define LCD_DC_Clr()   DL_GPIO_clearPins(LCD_DC_PORT, LCD_DC_PIN)
#define LCD_DC_Set()   DL_GPIO_setPins(LCD_DC_PORT, LCD_DC_PIN)

#define LCD_BLK_Clr()  DL_GPIO_clearPins(LCD_BLK_PORT, LCD_BLK_PIN)
#define LCD_BLK_Set()  DL_GPIO_setPins(LCD_BLK_PORT, LCD_BLK_PIN)


void LCD_GPIO_Init(void);
void LCD_Writ_Bus(u8 dat);
void LCD_WR_DATA8(u8 dat);
void LCD_WR_DATA(u16 dat);
void LCD_WR_REG(u8 dat);
void LCD_Address_Set(u16 x1, u16 y1, u16 x2, u16 y2);
void LCD_Init(void);

#endif
