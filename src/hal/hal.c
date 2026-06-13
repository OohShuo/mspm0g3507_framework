#include "hal.h"

#include "FreeRTOS.h"
#include "button.h"
#include "buzzer.h"
#include "com_uart.h"
#include "joystick.h"
#include "led_breath.h"
#include "led_simple.h"
#include "task.h"
#include "ws2812.h"

static TaskHandle_t task_gpio_handle = NULL;
static TaskHandle_t task_buzzer_handle = NULL;

static void task_gpio(void* arg);
static void task_buzzer(void* arg);

void Hal_Init(void) {
    Led_Simple_Init();
    Led_Breath_Init();
    Button_Init();
    Joystick_Init();
    Buzzer_Init();
    Com_Uart_Init();
    Ws2812_Init();
}

void Hal_Task_Def(void) {
    xTaskCreate(task_gpio, "Gpio_Task", 128, NULL, 1, &task_gpio_handle);
    xTaskCreate(task_buzzer, "Buzzer_Task", 128, NULL, 1, &task_buzzer_handle);
}

static void task_gpio(void* arg) {
    uint32_t tick = xTaskGetTickCount();
    while (1) {
        Led_Simple_Update_All();
        Led_Breath_Update_All();
        Button_Update_All();

        vTaskDelayUntil(&tick, pdMS_TO_TICKS(10));
    }
}

static void task_buzzer(void* arg) {
    uint32_t tick = xTaskGetTickCount();
    while (1) {
        Buzzer_Update_All();

        vTaskDelayUntil(&tick, pdMS_TO_TICKS(5));
    }
}
