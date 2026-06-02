#include "ti_msp_dl_config.h"
#include "FreeRTOS.h"
#include "task.h"

int tt = 0;

void vTaskLED(void *pvParameters) {
    while(1) {
        // 翻转 LED (请根据你的 SysConfig 命名修改)
        DL_GPIO_togglePins(GPIO_LEDS_PORT, GPIO_LEDS_USER_LED_PIN);
        tt++;
        vTaskDelay(pdMS_TO_TICKS(500)); // 延时 500ms
    }
}

int main(void) {
    SYSCFG_DL_init(); // 初始化 TI 硬件
    
    // 清除 SysTick 配置，FreeRTOS 会自己接管
    // 如果你在 SysConfig 里配置了定时器，请确保不要与 FreeRTOS 冲突
    
    xTaskCreate(vTaskLED, "LED_Task", 128, NULL, 1, NULL);
    
    vTaskStartScheduler();
    
    while(1);
}