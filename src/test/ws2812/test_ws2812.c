#include "test_ws2812.h"

#include "FreeRTOS.h"
#include "board_config.h"
#include "task.h"
#include "ws2812.h"

static Ws2812* g_ws2812 = NULL;

static void ws2812_task(void* arg) {
    (void)arg;

    const Ws2812_config cfg = {.rz_idx = RZ_WS2812_IDX, .led_count = 1};
    g_ws2812 = Ws2812_Create(&cfg);

    uint8_t phase = 0;
    while (1) {
        switch (phase) {
            case 0:
                Ws2812_Set_All(g_ws2812, 255, 0, 0);  // red
                break;
            case 1:
                Ws2812_Set_All(g_ws2812, 0, 255, 0);  // green
                break;
            case 2:
                Ws2812_Set_All(g_ws2812, 0, 0, 255);  // blue
                break;
            case 3:
                Ws2812_Set_All(g_ws2812, 0, 0, 0);  // white
                break;
            default:
                break;
        }
        Ws2812_Update(g_ws2812);

        phase = (phase + 1) & 3;
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void Test_Ws2812_Task_Def(void) { xTaskCreate(ws2812_task, "Ws2812", 128, NULL, 1, NULL); }
