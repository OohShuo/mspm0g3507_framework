#include "app.h"

#include "app_config.h"
#include "audio_player.h"
#include "flash_mgr.h"
#include "game_console.h"
#include "low_knight.h"
#include "storage.h"
#include "video_player.h"

void App_Init(void) {
#if FRAMEWORK_USE_LFS
    (void)Storage_Init();
#endif
}

void App_Task_Def(void) {
#if AUDIO_PLAYER_ENABLE
    Audio_Player_Task_Def();
#elif VIDEO_PLAYER_ENABLE
    Video_Player_Task_Def();
#elif LOW_KNIGHT_STANDALONE_ENABLE
    Low_Knight_Task_Def();
#elif FLASH_MGR_ENABLE
    Flash_Mgr_Task_Def();
#elif GAME_CONSOLE_ENABLE
    Game_Console_Task_Def();
#endif
}
