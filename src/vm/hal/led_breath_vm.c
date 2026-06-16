#include <stdlib.h>

#include "led_breath.h"

void Led_Breath_Init(void) {}

Led_breath* Led_Breath_Create(const Led_breath_config* c) {
    (void)c;
    return calloc(1, sizeof(Led_breath));
}
void Led_Breath_Set_Freq(Led_breath* o, float f) {
    (void)o;
    (void)f;
}
void Led_Breath_Update_All(void) {}
