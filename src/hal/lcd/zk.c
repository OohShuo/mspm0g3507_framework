#include "lcd_init.h"
#include "lcd.h"

/*
 * External font chip functions (require CS2 and MISO pins).
 * Not used on this hardware - CS is tied to GND, no font chip.
 * Define LCD_HAVE_FONT_CHIP in board_config.h and add CS2/FSO pin
 * macros to enable external font chip support.
 */
#ifdef LCD_HAVE_FONT_CHIP

u8 FontBuf[130];

void ZK_command(u8 dat)
{
    u8 i;
    for (i = 0; i < 8; i++) {
        LCD_SCLK_Clr();
        if (dat & 0x80) {
            LCD_MOSI_Set();
        } else {
            LCD_MOSI_Clr();
        }
        LCD_SCLK_Set();
        dat <<= 1;
    }
}

u8 get_data_from_ROM(void)
{
    u8 i;
    u8 ret_data = 0;
    for (i = 0; i < 8; i++) {
        LCD_SCLK_Clr();
        ret_data <<= 1;
        if (ZK_MISO) {
            ret_data++;
        }
        LCD_SCLK_Set();
    }
    return ret_data;
}

void get_n_bytes_data_from_ROM(u8 AddrHigh, u8 AddrMid, u8 AddrLow, u8 *pBuff, u8 DataLen)
{
    u8 i;
    ZK_CS_Clr();
    ZK_command(0x03);
    ZK_command(AddrHigh);
    ZK_command(AddrMid);
    ZK_command(AddrLow);
    for (i = 0; i < DataLen; i++) {
        *(pBuff + i) = get_data_from_ROM();
    }
    ZK_CS_Set();
}

#endif /* LCD_HAVE_FONT_CHIP */
