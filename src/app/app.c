#include "app.h"
#include "app_config.h"

#include "flash_mgr.h"
#include "pacman.h"

void App_Init(void) {}

void App_Task_Def(void) {
#if FLASH_MGR_ENABLE
    Flash_Mgr_Task_Def();
#endif
#if PACMAN_GAME_ENABLE
    Pacman_Task_Def();
#endif
}
