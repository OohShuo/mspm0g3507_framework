#include "test_button.h"

#include "FreeRTOS.h"
#include "board_config.h"
#include "button.h"
#include "task.h"

Button* g_button = NULL;

static void button_task(void* arg) {
    (void)arg;
    const Button_config cfg = {
        .gpio_idx = GPIO_SW_BTN_IDX,
        .gpio_state_when_pressed = bsp_gpio_state_reset,
    };
    g_button = Button_Create(&cfg);
    while (1) { vTaskDelay(pdMS_TO_TICKS(5000)); }
}

void Test_Button_Task_Def(void) { xTaskCreate(button_task, "Button", 128, NULL, 1, NULL); }
