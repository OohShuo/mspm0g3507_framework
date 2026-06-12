#include "test_rtt.h"

#include "FreeRTOS.h"
#include "bsp_time.h"
#include "rtt_log.h"
#include "task.h"

static void rtt_init(void) {
    printf(
        "Hello from RTT test app! This confirms that the RTT control block is working and can be used for "
        "logging.\n");
}

static int tick = 0;
static void rtt_loop(void) {
    tick = Bsp_Get_Tick_Ms();
    printf("RTT Test Loop: tick = %d ms, time = %d.%03d s\n", tick, tick / 1000, tick % 1000);
}

static void rtt_test_task(void* arg) {
    (void)arg;
    rtt_init();
    uint32_t tick = xTaskGetTickCount();
    while (1) {
        rtt_loop();
        vTaskDelayUntil(&tick, pdMS_TO_TICKS(1000));
    }
}

void Test_Rtt_Task_Def(void) { xTaskCreate(rtt_test_task, "RTT_Test", 512, NULL, 1, NULL); }
