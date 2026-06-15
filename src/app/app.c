#include "app.h"

#include "app_config.h"
#include "flash_mgr.h"
#include "game_console.h"
#include "low_knight.h"
#include "storage.h"

void App_Init(void) {
#if FRAMEWORK_USE_LFS
    (void)Storage_Init();
#endif
}

void App_Task_Def(void) {
#if LOW_KNIGHT_STANDALONE_ENABLE
    Low_Knight_Task_Def();
#else
#if FLASH_MGR_ENABLE
    Flash_Mgr_Task_Def();
#endif
#if GAME_CONSOLE_ENABLE
    Game_Console_Task_Def();
#endif
#endif
}
