#include "app.h"

#include "app_config.h"
#include "flash_mgr.h"
#include "game_console.h"
#include "storage.h"

void App_Init(void) {
#if FRAMEWORK_USE_LFS
    (void)Storage_Init();
#endif
}

void App_Task_Def(void) {
#if FLASH_MGR_ENABLE
    Flash_Mgr_Task_Def();
#endif
#if GAME_CONSOLE_ENABLE
    Game_Console_Task_Def();
#endif
}
