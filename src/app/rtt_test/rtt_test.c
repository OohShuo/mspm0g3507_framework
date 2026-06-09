#include "rtt_test.h"

#include <stdio.h>

#include "bsp_time.h"

void Rtt_Test_Init(void) {
    printf("Hello from RTT test app! This confirms that the RTT control block is working and can be used for logging.\n");
}

static uint32_t tick = 0;
static float time = 0;
void Rtt_Test_Loop(void) {
    tick = Bsp_Get_Tick_Ms();
    time = tick / 1000.0f;
    printf("RTT Test Loop: tick = %u ms, time = %.2f s\n", tick, time);
    printf("@%s %d %s\n", __FILE__, __LINE__, __func__);
}