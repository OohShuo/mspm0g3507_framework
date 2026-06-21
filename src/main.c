#include "app.h"
#include "bsp.h"
#include "hal.h"
#include "local_lib.h"
#include "platform.h"
#include "retarget.h"
#include "test.h"
#include "test_config.h"

int main(void) {
    if (Platform_Init() != 0) { return 1; }

    Syscall_Init();

    Local_Lib_Init();
    Bsp_Init();
    Hal_Init();
    App_Init();

    Hal_Task_Def();
#if TEST_ANY_ENABLE
    Test_Task_Def();
#endif
    App_Task_Def();

    return Platform_Start();
}
