#include "test_led_simple.h"

#include "FreeRTOS.h"
#include "board_config.h"
#include "led_simple.h"
#include "task.h"

Led_simple* g_led_simple = NULL;

static void led_simple_task(void* arg) {
    (void)arg;
    const Led_simple_config cfg = {
        .gpio_idx = GPIO_TFT_BLK_IDX,
        .gpio_state_when_on = bsp_gpio_state_reset,
    };
    g_led_simple = Led_Simple_Create(&cfg);
    while (1) {
        Led_Simple_Toggle(g_led_simple);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void Test_Led_Simple_Task_Def(void) { xTaskCreate(led_simple_task, "LED_Simple", 128, NULL, 1, NULL); }
