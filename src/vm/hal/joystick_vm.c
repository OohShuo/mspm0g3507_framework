#include "joystick.h"
#include "bsp_adc.h"
#include "input_vm.h"
#include <stdlib.h>
#include <string.h>

static Joystick* g_joy = NULL;

void Joystick_Init(void) {}

Joystick* Joystick_Create(const Joystick_config* c) {
    if (!c || g_joy) return NULL;
    g_joy = calloc(1, sizeof(Joystick));
    if (!g_joy) return NULL;
    g_joy->config = *c;
    g_joy->x_center_voltage = (c->x_min_voltage + c->x_max_voltage) * 0.5f;
    g_joy->y_center_voltage = (c->y_min_voltage + c->y_max_voltage) * 0.5f;

    // VM: override hardware-specific config for intuitive WASD mapping
    g_joy->config.y_reverse = 0;   // W=up, S=down (no physical inversion)
    g_joy->config.x_reverse = 0;   // A=left, D=right

    // Start the virtual ADC thread that calls Vm_Joystick_Update() every 10ms
    Bsp_Adc_Start(c->adc_idx);
    return g_joy;
}

void Joystick_Calibrate_Center(Joystick* o, uint32_t n, uint32_t ms) {
    (void)o; (void)n; (void)ms; // VM: center is always 0,0 from keyboard
}

// Called periodically by the VM ADC thread to update joystick values from keyboard
void Vm_Joystick_Update(void) {
    if (!g_joy) return;

    // Read keyboard → voltage → normalized value (matching joystick.c logic)
    float x_raw = Vm_Input_Get_X();
    float y_raw = Vm_Input_Get_Y();

    // Simulate ADC voltage from keyboard value
    float x_v = 1.65f + x_raw * 1.65f;
    float y_v = 1.65f + y_raw * 1.65f;

    // normalize_axis (inline)
    const Joystick_config* c = &g_joy->config;
    float xc = g_joy->x_center_voltage;
    float yc = g_joy->y_center_voltage;

    if (x_v >= xc) g_joy->x_value = (x_v - xc) / (c->x_max_voltage - xc);
    else           g_joy->x_value = (x_v - xc) / (xc - c->x_min_voltage);
    if (g_joy->x_value > 1.0f) g_joy->x_value = 1.0f;
    if (g_joy->x_value < -1.0f) g_joy->x_value = -1.0f;

    if (y_v >= yc) g_joy->y_value = (y_v - yc) / (c->y_max_voltage - yc);
    else           g_joy->y_value = (y_v - yc) / (yc - c->y_min_voltage);
    if (g_joy->y_value > 1.0f) g_joy->y_value = 1.0f;
    if (g_joy->y_value < -1.0f) g_joy->y_value = -1.0f;

    g_joy->x_value -= c->x_offset;
    g_joy->y_value -= c->y_offset;
    if (c->x_reverse) g_joy->x_value *= -1;
    if (c->y_reverse) g_joy->y_value *= -1;

    // apply_dead_zone (inline)
    float dz = c->x_dead_zone;
    float ax = g_joy->x_value < 0 ? -g_joy->x_value : g_joy->x_value;
    if (ax <= dz) g_joy->x_value = 0;
    else { float s = (ax - dz) / (1.0f - dz); g_joy->x_value = g_joy->x_value < 0 ? -s : s; }

    dz = c->y_dead_zone;
    float ay = g_joy->y_value < 0 ? -g_joy->y_value : g_joy->y_value;
    if (ay <= dz) g_joy->y_value = 0;
    else { float s = (ay - dz) / (1.0f - dz); g_joy->y_value = g_joy->y_value < 0 ? -s : s; }
}
