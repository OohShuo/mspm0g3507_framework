#include "game_registry.h"

#include <stddef.h>

#define game_entry(name) extern const Game_descriptor game_##name##_entry;
#include "game_entries.inc"
#undef game_entry

static const Game_descriptor* const g_games[] = {
#define game_entry(name) &game_##name##_entry,
#include "game_entries.inc"
#undef game_entry
};

typedef char
    Game_registry_count_matches_ids[(sizeof(g_games) / sizeof(g_games[0]) == game_id_count) ? 1 : -1];

uint8_t Game_Registry_Count(void) { return (uint8_t)(sizeof(g_games) / sizeof(g_games[0])); }

const Game_descriptor* Game_Registry_Get(uint8_t index) {
    return index < Game_Registry_Count() ? g_games[index] : NULL;
}
