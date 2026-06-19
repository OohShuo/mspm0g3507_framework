#include "bsp_adc.h"

#include <SDL2/SDL.h>
#include <stdio.h>

#include "input_vm.h"

// External: Vm_Joystick_Update is in joystick_vm.c
extern void Vm_Joystick_Update(void);

static SDL_Thread* g_thread = NULL;
static volatile int g_running = 0;

static int adc_thread(void* a) {
    (void)a;
    while (g_running) {
        Vm_Joystick_Update();
        SDL_Delay(10);
    }
    return 0;
}

void Bsp_Adc_Init(void) {}
void Bsp_Adc_Start(uint32_t i) {
    (void)i;
    if (!g_running) {
        g_running = 1;
        g_thread = SDL_CreateThread(adc_thread, "vm-adc", NULL);
        if (g_thread != NULL) { SDL_DetachThread(g_thread); }
    }
}
void Bsp_Adc_Stop(uint32_t i) {
    (void)i;
    g_running = 0;
    g_thread = NULL;
}
float Bsp_Adc_Read_Voltage(uint32_t i, uint32_t ch) {
    (void)i;
    float v = (ch == 0) ? Vm_Input_Get_X() : Vm_Input_Get_Y();
    return 1.65f + v * 1.65f;
}
void Bsp_Adc_Register_Cb_Dma_Done(uint32_t i, Bsp_adc_cb_t cb, void* a) {
    (void)i;
    (void)cb;
    (void)a;
}
