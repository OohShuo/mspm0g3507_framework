#include "ti_msp_dl_config.h"
#include "FreeRTOS.h"
#include "task.h"

int tt = 0;

static void task_led(void *pvParameters) {
    while(1) {
        DL_GPIO_togglePins(GPIO_LEDS_PORT, GPIO_LEDS_USER_LED_PIN);
        tt++;
        vTaskDelay(pdMS_TO_TICKS(1000)); // 延时 1000ms
    }
}

int main(void) {
    SYSCFG_DL_init();
    
    xTaskCreate(task_led, "LED_Task", 128, NULL, 1, NULL);
    
    vTaskStartScheduler();
    
    while(1);
}