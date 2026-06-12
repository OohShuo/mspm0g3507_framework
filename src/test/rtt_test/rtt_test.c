#include "rtt_test.h"

#include "FreeRTOS.h"
#include "bsp_time.h"
#include "rtt_log.h"
#include "task.h"

void Rtt_Test_Init(void) {
    printf(
        "Hello from RTT test app! This confirms that the RTT control block is working and can be used for "
        "logging.\n");
}

static int tick = 0;
void Rtt_Test_Loop(void) {
    tick = Bsp_Get_Tick_Ms();
    printf("RTT Test Loop: tick = %d ms, time = %d.%03d s\n", tick, tick / 1000, tick % 1000);
}

static void rtt_test_task(void* arg) {
    (void)arg;
    Rtt_Test_Init();
    uint32_t tick = xTaskGetTickCount();
    while (1) {
        Rtt_Test_Loop();
        vTaskDelayUntil(&tick, pdMS_TO_TICKS(1000));
    }
}

void Rtt_Test_Task_Def(void) { xTaskCreate(rtt_test_task, "RTT_Test", 512, NULL, 1, NULL); }
