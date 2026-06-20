#include "test_button.h"

#include "FreeRTOS.h"
#include "board_config.h"
#include "button.h"
#include "rtt_log.h"
#include "task.h"

typedef struct {
    const char* name;
    uint32_t gpio_idx;
    Button* button;
    Button_state last_state;
} Test_button;

static Test_button g_buttons[] = {
    {.name = "A", .gpio_idx = GPIO_BNT_DOWN_IDX},
    {.name = "B", .gpio_idx = GPIO_BNT_RIGHT_IDX},
    {.name = "X", .gpio_idx = GPIO_BNT_UP_IDX},
    {.name = "Y", .gpio_idx = GPIO_BNT_LEFT_IDX},
    {.name = "START", .gpio_idx = GPIO_SW_BTN_IDX},
};

static void button_task(void* arg) {
    (void)arg;
    for (uint32_t i = 0; i < sizeof(g_buttons) / sizeof(g_buttons[0]); i++) {
        const Button_config cfg = {
            .gpio_idx = g_buttons[i].gpio_idx,
            .gpio_state_when_pressed = bsp_gpio_state_reset,
        };
        g_buttons[i].button = Button_Create(&cfg);
        configASSERT(g_buttons[i].button != NULL);
    }

    while (1) {
        for (uint32_t i = 0; i < sizeof(g_buttons) / sizeof(g_buttons[0]); i++) {
            const Button_state state = Button_Get_State(g_buttons[i].button);
            if (state != g_buttons[i].last_state) {
                printf("[INPUT] %s %s\n", g_buttons[i].name, state == button_state_down ? "DOWN" : "UP");
                g_buttons[i].last_state = state;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void Test_Button_Task_Def(void) { xTaskCreate(button_task, "Button", 128, NULL, 1, NULL); }
