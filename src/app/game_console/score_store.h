#pragma once

#include <stdint.h>

#define SCORE_STORE_NAME_LENGTH 6u
#define SCORE_STORE_TOP_COUNT   5u
#define SCORE_STORE_MAX_GAMES   24u

typedef struct {
    char name[SCORE_STORE_NAME_LENGTH + 1u];
    uint8_t reserved;
    uint32_t score;
} Score_entry;

uint8_t Score_Store_Map_Legacy_Id(uint8_t legacy_id);
void Score_Store_Migrate_Legacy_Entries(uint8_t legacy_game_count,
    const uint8_t legacy_entry_count[SCORE_STORE_MAX_GAMES],
    const Score_entry legacy_entries[SCORE_STORE_MAX_GAMES][SCORE_STORE_TOP_COUNT],
    uint8_t current_game_count, uint8_t current_entry_count[SCORE_STORE_MAX_GAMES],
    Score_entry current_entries[SCORE_STORE_MAX_GAMES][SCORE_STORE_TOP_COUNT]);

void Score_Store_Init(uint8_t game_count);
uint8_t Score_Store_Is_Available(void);
uint32_t Score_Store_Get(uint8_t game_index);
uint8_t Score_Store_Qualifies(uint8_t game_index, uint32_t score);
uint8_t Score_Store_Add(uint8_t game_index, const char* name, uint32_t score);
uint8_t Score_Store_Get_Count(uint8_t game_index);
const Score_entry* Score_Store_Get_Entry(uint8_t game_index, uint8_t rank);
void Score_Store_Commit(void);
