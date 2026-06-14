#pragma once

#define FLASH_MGR_ENABLE 0
#define GAME_CONSOLE_ENABLE 1
#define GAME_RUNTIME_MONITOR_ENABLE 1
#define GAME_RUNTIME_MONITOR_INTERVAL_MS 5000u

/* Fall back to the built-in background if external image restoration is too slow. */
#define AIR_BATTLE_EXTERNAL_BG_MAX_FLUSH_MS 100u
#define AIR_BATTLE_EXTERNAL_BG_SLOW_LIMIT   2u
