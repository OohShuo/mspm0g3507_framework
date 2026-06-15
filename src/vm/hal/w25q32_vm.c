#include "w25q32.h"
#include <string.h>

static W25q32 s_flash;

W25q32* W25q32_Create(const W25q32_config* c) { if(!c)return NULL; memset(&s_flash,0,sizeof(s_flash)); s_flash.config=*c; s_flash.manufacturer_id=0xEF; s_flash.memory_type=0x40; s_flash.capacity=0x17; return &s_flash; }
uint8_t W25q32_Init(W25q32* o)                             { (void)o; return 1; }
void    W25q32_Read_Jedec_Id(W25q32* o, uint8_t* id)       { (void)o; if(id){id[0]=0xEF;id[1]=0x40;id[2]=0x17;} }
uint8_t W25q32_Read_Status_Reg_1(W25q32* o)                { (void)o; return 0; }
void    W25q32_Write_Enable(W25q32* o)                      { (void)o; }
void    W25q32_Write_Status_Reg_1(W25q32* o, uint8_t s)     { (void)o;(void)s; }
void    W25q32_Wait_Busy(W25q32* o)                         { (void)o; }
void    W25q32_Read(W25q32* o, uint32_t a, uint8_t* d, uint32_t l) { (void)o;(void)a; if(d)memset(d,0xFF,l); }
void    W25q32_Page_Program(W25q32* o, uint32_t a, const uint8_t* d, uint32_t l) { (void)o;(void)a;(void)d;(void)l; }
void    W25q32_Sector_Erase(W25q32* o, uint32_t a)         { (void)o;(void)a; }
void    W25q32_Block_Erase_32K(W25q32* o, uint32_t a)      { (void)o;(void)a; }
void    W25q32_Block_Erase_64K(W25q32* o, uint32_t a)      { (void)o;(void)a; }
void    W25q32_Chip_Erase(W25q32* o)                        { (void)o; }
void    W25q32_Power_Down(W25q32* o)                        { (void)o; }
void    W25q32_Release_Power_Down(W25q32* o)                { (void)o; }
void    W25q32_Reset(W25q32* o)                             { (void)o; }
