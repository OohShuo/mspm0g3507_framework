#pragma once

#define FLASH_MGR_ENABLE                 0
#define GAME_CONSOLE_ENABLE              1
#define GAME_RUNTIME_MONITOR_ENABLE      0
#define GAME_RUNTIME_MONITOR_INTERVAL_MS 5000u

/* Raw cache slot in the W25Q32 low 2 MiB reserved resource area. */
#define AIR_BATTLE_BG_CACHE_ADDRESS      (1u * 1024u * 1024u)
#define AIR_BATTLE_BG_CACHE_CAPACITY     (256u * 1024u)
