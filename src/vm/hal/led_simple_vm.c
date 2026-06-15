#include "led_simple.h"
#include <stdlib.h>

static Led_simple* g_leds[4] = {NULL};
static int g_led_count = 0;

void Led_Simple_Init(void) {}

Led_simple* Led_Simple_Create(const Led_simple_config* c) {
    if (!c || g_led_count >= 4) return NULL;
    Led_simple* l = calloc(1, sizeof(Led_simple));
    if (!l) return NULL;
    l->config = *c;
    l->state = led_simple_state_off;
    g_leds[g_led_count++] = l;
    return l;
}

void Led_Simple_Set_State(Led_simple* o, Led_simple_state s)   { if(o) o->state=s; }
void Led_Simple_Toggle(Led_simple* o) { if(o) o->state = (o->state==led_simple_state_on)?led_simple_state_off:led_simple_state_on; }
void Led_Simple_Set_Blink_Freq(Led_simple* o, uint8_t f)       { if(o) o->blink_freq_hz=f; }
void Led_Simple_Update_All(void) {
    for (int i = 0; i < g_led_count; i++) {
        Led_simple* l = g_leds[i];
        if (!l || !l->blink_freq_hz) continue;
        // Simple blink: toggle every (1000/freq/2) ms
        // For VM, just no-op — LEDs aren't visible anyway
    }
}
