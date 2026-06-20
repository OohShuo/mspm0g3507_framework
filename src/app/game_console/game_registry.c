#include "game_registry.h"

#include <stddef.h>

#include "air_battle.h"
#include "breakout.h"
#include "calculator.h"
#include "dino_runner.h"
#include "dodge_box.h"
#include "flappy_bird.h"
#include "fps_test.h"
#include "game_2048.h"
#include "gomoku.h"
#include "info.h"
#include "maze.h"
#include "needle.h"
#include "pacman.h"
#include "pong.h"
#include "rhythm.h"
#include "sfx_lib.h"
#include "snake.h"
#include "tank_battle.h"
#include "tetris.h"
#include "volume_control.h"

static const Game_descriptor g_games[] = {
    {
        .name = "PAC-MAN",
        .icon = game_icon_pacman,
        .id = game_id_pacman,
        .control_hint = NULL,
        .is_game = 1,
        .init = Pacman_Init,
        .update = Pacman_Update,
        .get_score = Pacman_Get_Score,
        .is_finished = Pacman_Is_Finished,
    },
    {
        .name = "SNAKE",
        .icon = game_icon_snake,
        .id = game_id_snake,
        .control_hint = NULL,
        .is_game = 1,
        .init = Snake_Init,
        .update = Snake_Update,
        .get_score = Snake_Get_Score,
        .is_finished = Snake_Is_Finished,
    },
    {
        .name = "TANK",
        .icon = game_icon_tank,
        .id = game_id_tank,
        .control_hint = "A FIRE",
        .is_game = 1,
        .init = Tank_Battle_Init,
        .update = Tank_Battle_Update,
        .get_score = Tank_Battle_Get_Score,
        .is_finished = Tank_Battle_Is_Finished,
    },
    {
        .name = "AIR FORCE",
        .icon = game_icon_air,
        .id = game_id_air,
        .control_hint = "A BOMB",
        .is_game = 1,
        .init = Air_Battle_Init,
        .update = Air_Battle_Update,
        .get_score = Air_Battle_Get_Score,
        .is_finished = Air_Battle_Is_Finished,
    },
    {
        .name = "TETRIS",
        .icon = game_icon_tetris,
        .id = game_id_tetris,
        .control_hint = "A ROTATE Y DROP",
        .is_game = 1,
        .init = Tetris_Init,
        .update = Tetris_Update,
        .get_score = Tetris_Get_Score,
        .is_finished = Tetris_Is_Finished,
    },
    {
        .name = "BREAKOUT",
        .icon = game_icon_breakout,
        .id = game_id_breakout,
        .control_hint = "A LAUNCH",
        .is_game = 1,
        .init = Breakout_Init,
        .update = Breakout_Update,
        .get_score = Breakout_Get_Score,
        .is_finished = Breakout_Is_Finished,
    },
    {
        .name = "PONG",
        .icon = game_icon_pong,
        .id = game_id_pong,
        .control_hint = "A SERVE",
        .is_game = 1,
        .init = Pong_Init,
        .update = Pong_Update,
        .get_score = Pong_Get_Score,
        .is_finished = Pong_Is_Finished,
    },
    {
        .name = "GOMOKU",
        .icon = game_icon_gomoku,
        .id = game_id_gomoku,
        .control_hint = "A PLACE",
        .is_game = 1,
        .init = Gomoku_Init,
        .update = Gomoku_Update,
        .get_score = Gomoku_Get_Score,
        .is_finished = Gomoku_Is_Finished,
    },
    {
        .name = "2048",
        .icon = game_icon_2048,
        .id = game_id_2048,
        .control_hint = NULL,
        .is_game = 1,
        .init = Game_2048_Init,
        .update = Game_2048_Update,
        .get_score = Game_2048_Get_Score,
        .is_finished = Game_2048_Is_Finished,
    },
    {
        .name = "DINO",
        .icon = game_icon_dino,
        .id = game_id_dino,
        .control_hint = "A JUMP Y DUCK",
        .is_game = 1,
        .init = Dino_Runner_Init,
        .update = Dino_Runner_Update,
        .get_score = Dino_Runner_Get_Score,
        .is_finished = Dino_Runner_Is_Finished,
    },
    {
        .name = "FLAPPY",
        .icon = game_icon_flappy,
        .id = game_id_flappy,
        .control_hint = "A FLAP Y GLIDE",
        .is_game = 1,
        .init = Flappy_Bird_Init,
        .update = Flappy_Bird_Update,
        .get_score = Flappy_Bird_Get_Score,
        .is_finished = Flappy_Bird_Is_Finished,
    },
    {
        .name = "MAZE",
        .icon = game_icon_maze,
        .id = game_id_maze,
        .control_hint = NULL,
        .is_game = 1,
        .init = Maze_Init,
        .update = Maze_Update,
        .get_score = Maze_Get_Score,
        .is_finished = Maze_Is_Finished,
    },
    {
        .name = "NEEDLE",
        .icon = game_icon_needle,
        .id = game_id_needle,
        .control_hint = "A LAUNCH Y QUICK",
        .is_game = 1,
        .init = Needle_Init,
        .update = Needle_Update,
        .get_score = Needle_Get_Score,
        .is_finished = Needle_Is_Finished,
    },
    {
        .name = "DODGE",
        .icon = game_icon_dodge_box,
        .id = game_id_dodge_box,
        .control_hint = NULL,
        .is_game = 1,
        .init = Dodge_Box_Init,
        .update = Dodge_Box_Update,
        .get_score = Dodge_Box_Get_Score,
        .is_finished = Dodge_Box_Is_Finished,
    },
    {
        .name = "RHYTHM",
        .icon = game_icon_rhythm,
        .id = game_id_rhythm,
        .control_hint = "A START",
        .is_game = 1,
        .init = Rhythm_Init,
        .update = Rhythm_Update,
        .get_score = Rhythm_Get_Score,
        .is_finished = Rhythm_Is_Finished,
    },
    {
        .name = "SFX",
        .icon = game_icon_sfx_lib,
        .id = game_id_sfx_lib,
        .control_hint = NULL,
        .init = Sfx_Lib_Init,
        .update = Sfx_Lib_Update,
        .get_score = Sfx_Lib_Get_Score,
        .is_finished = Sfx_Lib_Is_Finished,
    },
    {
        .name = "CALC",
        .icon = game_icon_calculator,
        .id = game_id_calculator,
        .control_hint = NULL,
        .init = Calc_Init,
        .update = Calc_Update,
        .get_score = Calc_Get_Score,
        .is_finished = Calc_Is_Finished,
    },
    {
        .name = "FPS TEST",
        .icon = game_icon_fps_test,
        .id = game_id_fps_test,
        .control_hint = NULL,
        .init = Fps_Test_Init,
        .update = Fps_Test_Update,
        .get_score = Fps_Test_Get_Score,
        .is_finished = Fps_Test_Is_Finished,
    },

    {
        .name = "VOLUME",
        .icon = game_icon_volume_control,
        .id = game_id_volume_control,
        .control_hint = NULL,
        .init = Volume_Control_Init,
        .update = Volume_Control_Update,
        .get_score = Volume_Control_Get_Score,
        .is_finished = Volume_Control_Is_Finished,
    },
    {
        .name = "INFO",
        .icon = game_icon_info,
        .id = game_id_info,
        .control_hint = NULL,
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
