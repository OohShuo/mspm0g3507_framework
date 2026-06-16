#include "game_registry.h"

#include <stddef.h>

#include "air_battle.h"
#include "breakout.h"
#include "dino_runner.h"
#include "flappy_bird.h"
#include "fps_test.h"
#include "game_2048.h"
#include "gomoku.h"
#include "info.h"
#include "maze.h"
#include "needle.h"
#include "pacman.h"
#include "pong.h"
#include "racing.h"
#include "snake.h"
#include "tank_battle.h"
#include "tetris.h"

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
    {
        .name = "TETRIS",
        .icon = game_icon_tetris,
        .id = game_id_tetris,
        .init = Tetris_Init,
        .update = Tetris_Update,
        .get_score = Tetris_Get_Score,
        .is_finished = Tetris_Is_Finished,
    },
    {
        .name = "BREAKOUT",
        .icon = game_icon_breakout,
        .id = game_id_breakout,
        .init = Breakout_Init,
        .update = Breakout_Update,
        .get_score = Breakout_Get_Score,
        .is_finished = Breakout_Is_Finished,
    },
    {
        .name = "PONG",
        .icon = game_icon_pong,
        .id = game_id_pong,
        .init = Pong_Init,
        .update = Pong_Update,
        .get_score = Pong_Get_Score,
        .is_finished = Pong_Is_Finished,
    },
    {
        .name = "GOMOKU",
        .icon = game_icon_gomoku,
        .id = game_id_gomoku,
        .init = Gomoku_Init,
        .update = Gomoku_Update,
        .get_score = Gomoku_Get_Score,
        .is_finished = Gomoku_Is_Finished,
    },
    {
        .name = "2048",
        .icon = game_icon_2048,
        .id = game_id_2048,
        .init = Game_2048_Init,
        .update = Game_2048_Update,
        .get_score = Game_2048_Get_Score,
        .is_finished = Game_2048_Is_Finished,
    },
    {
        .name = "DINO",
        .icon = game_icon_dino,
        .id = game_id_dino,
        .init = Dino_Runner_Init,
        .update = Dino_Runner_Update,
        .get_score = Dino_Runner_Get_Score,
        .is_finished = Dino_Runner_Is_Finished,
    },
    {
        .name = "FLAPPY",
        .icon = game_icon_flappy,
        .id = game_id_flappy,
        .init = Flappy_Bird_Init,
        .update = Flappy_Bird_Update,
        .get_score = Flappy_Bird_Get_Score,
        .is_finished = Flappy_Bird_Is_Finished,
    },
    {
        .name = "MAZE",
        .icon = game_icon_maze,
        .id = game_id_maze,
        .init = Maze_Init,
        .update = Maze_Update,
        .get_score = Maze_Get_Score,
        .is_finished = Maze_Is_Finished,
    },
    {
        .name = "NEEDLE",
        .icon = game_icon_needle,
        .id = game_id_needle,
        .init = Needle_Init,
        .update = Needle_Update,
        .get_score = Needle_Get_Score,
        .is_finished = Needle_Is_Finished,
    },
    {
        .name = "FPS TEST",
        .icon = game_icon_fps_test,
        .id = game_id_fps_test,
        .init = Fps_Test_Init,
        .update = Fps_Test_Update,
        .get_score = Fps_Test_Get_Score,
        .is_finished = Fps_Test_Is_Finished,
    },
    {
        .name = "INFO",
        .icon = game_icon_info,
        .id = game_id_info,
        .init = Info_Init,
        .update = Info_Update,
        .get_score = Info_Get_Score,
        .is_finished = Info_Is_Finished,
    },
};

uint8_t Game_Registry_Count(void) { return (uint8_t)(sizeof(g_games) / sizeof(g_games[0])); }

const Game_descriptor* Game_Registry_Get(uint8_t index) {
    return index < Game_Registry_Count() ? &g_games[index] : NULL;
}
