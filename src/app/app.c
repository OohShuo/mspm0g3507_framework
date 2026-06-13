#include "app.h"
#include "app_config.h"

#include "flash_mgr.h"

void App_Init(void) {}

void App_Task_Def(void) { 
    #if FLASH_MGR_ENABLE
    Flash_Mgr_Task_Def(); 
    #endif
}
