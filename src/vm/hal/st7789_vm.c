#include <string.h>

#include "display_vm.h"
#include "st7789.h"

static St7789 s_lcd;

St7789* St7789_Create(const St7789_config* c) {
    if (!c) return NULL;
    memset(&s_lcd, 0, sizeof(s_lcd));
    s_lcd.config = *c;
    return &s_lcd;
}
void St7789_Reset(St7789* o) { (void)o; }
void St7789_Run_Init_Sequence(St7789* o) { (void)o; }
void St7789_Init(St7789* o) { (void)o; }
void St7789_Set_Backlight(St7789* o, uint8_t on) {
    (void)o;
    (void)on;
}
void St7789_Send_Cmd(St7789* o, const uint8_t* cmd, uint32_t cl, const uint8_t* p, uint32_t pl) {
    (void)o;
    (void)cmd;
    (void)cl;
    (void)p;
    (void)pl;
}
void St7789_Register_Flush_Done_Cb(St7789* o, St7789_flush_done_cb cb, void* a) {
    if (o) {
        o->flush_done_cb = cb;
        o->flush_done_cb_arg = a;
    }
}

static struct {
    int16_t x1, y1, x2, y2;
    uint8_t on;
} g_ws;

void St7789_Begin_Write(St7789* o, int32_t x1, int32_t y1, int32_t x2, int32_t y2) {
    (void)o;
    g_ws.x1 = (int16_t)x1;
    g_ws.y1 = (int16_t)y1;
    g_ws.x2 = (int16_t)x2;
    g_ws.y2 = (int16_t)y2;
    g_ws.on = 1;
}
void St7789_Write_Pixels(St7789* o, uint8_t* px, uint32_t len) {
    (void)o;
    if (!g_ws.on || !px || len < 2) return;
    uint32_t n = len / 2, w = 0;
    int16_t ww = g_ws.x2 - g_ws.x1 + 1, cy = g_ws.y1;
    const uint16_t* p = (const uint16_t*)px;
    while (w < n && cy <= g_ws.y2) {
        uint32_t r = n - w, need = (uint32_t)ww, tw = r < need ? r : need;
        Vm_Display_Write_Pixels(g_ws.x1, cy, g_ws.x1 + (int16_t)tw - 1, cy, (const uint8_t*)(p + w), tw);
        w += tw;
        cy++;
    }
    g_ws.y1 = cy;
}
void St7789_End_Write(St7789* o) {
    g_ws.on = 0;
    Vm_Display_Frame_Done();
    if (o && o->flush_done_cb) o->flush_done_cb(o->flush_done_cb_arg);
}
void St7789_Flush(St7789* o, int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint8_t* px, uint32_t sz) {
    St7789_Begin_Write(o, x1, y1, x2, y2);
    St7789_Write_Pixels(o, px, sz);
    St7789_End_Write(o);
}
void St7789_Send_Color(St7789* o, const uint8_t* cmd, uint32_t cl, uint8_t* px, uint32_t pl) {
    (void)o;
    (void)cmd;
    (void)cl;
    if (px && pl) St7789_Write_Pixels(o, px, pl);
}
