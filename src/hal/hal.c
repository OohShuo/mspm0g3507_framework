#include "hal.h"

#include "button.h"
#include "buzzer.h"
#include "led_breath.h"
#include "led_simple.h"

void Hal_Init(void) {
    Led_Simple_Init();
    Led_Breath_Init();
    Button_Init();
    Buzzer_Init();
}

void Hal_Gpio_Loop(void) {
    Led_Simple_Update_All();
    Led_Breath_Update_All();
    Button_Update_All();
}

void Hal_Buzzer_Loop(void) { Buzzer_Update_All(); }