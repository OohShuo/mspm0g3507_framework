#include "game_registry.h"

#include <stddef.h>

#include "air_battle.h"
#include "pacman.h"
#include "racing.h"
#include "snake.h"
#include "tank_battle.h"

static const Game_descriptor g_games[] = {
    {
        .name = "PAC-MAN",
        .icon = game_icon_pacman,
        .id = game_id_pacman,
        .init = Pacman_Init,
        .update = Pacman_Update,
        .get_score = Pacman_Get_Score,
        .is_finished = Pacman_Is_Finished,
    },
    {
        .name = "SNAKE",
        .icon = game_icon_snake,
        .id = game_id_snake,
        .init = Snake_Init,
        .update = Snake_Update,
        .get_score = Snake_Get_Score,
        .is_finished = Snake_Is_Finished,
    },
    {
        .name = "RACING",
        .icon = game_icon_racing,
        .id = game_id_racing,
        .init = Racing_Init,
        .update = Racing_Update,
        .get_score = Racing_Get_Score,
        .is_finished = Racing_Is_Finished,
    },
    {
        .name = "TANK",
        .icon = game_icon_tank,
        .id = game_id_tank,
        .init = Tank_Battle_Init,
        .update = Tank_Battle_Update,
        .get_score = Tank_Battle_Get_Score,
        .is_finished = Tank_Battle_Is_Finished,
    },
    {
        .name = "AIR FORCE",
        .icon = game_icon_air,
        .id = game_id_air,
        .init = Air_Battle_Init,
        .update = Air_Battle_Update,
        .get_score = Air_Battle_Get_Score,
        .is_finished = Air_Battle_Is_Finished,
    },
};

uint8_t Game_Registry_Count(void) { return (uint8_t)(sizeof(g_games) / sizeof(g_games[0])); }

const Game_descriptor* Game_Registry_Get(uint8_t index) {
    return index < Game_Registry_Count() ? &g_games[index] : NULL;
}
