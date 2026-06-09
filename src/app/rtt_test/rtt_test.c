#include "rtt_test.h"

#include "rtt_log.h"
#include "bsp_time.h"

void Rtt_Test_Init(void) {
    printf("Hello from RTT test app! This confirms that the RTT control block is working and can be used for logging.\n");
}

static int tick = 0;
void Rtt_Test_Loop(void) {
    tick = Bsp_Get_Tick_Ms();
    printf("RTT Test Loop: tick = %d ms, time = %d.%03d s\n", tick, tick / 1000, tick % 1000);
}
