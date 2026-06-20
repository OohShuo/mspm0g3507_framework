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
        .info_text = "DESCRIPTION\nClassic maze chase.\nEat dots while ghosts hunt you.\n\nGOAL\nClear the "
                     "maze and stay alive.\n\nCONTROLS\nJOY MOVE\nX/B PAUSE",
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
        .info_text = "DESCRIPTION\nGuide a growing snake.\nEat food and avoid every wall.\n\nGOAL\nGrow as "
                     "long as you can.\n\nCONTROLS\nJOY TURN\nX/B PAUSE",
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
        .info_text = "DESCRIPTION\nTop-down tank combat.\nEnemy tanks patrol the field.\n\nGOAL\nDestroy all "
                     "enemies and survive.\n\nCONTROLS\nJOY MOVE AND AIM\nA FIRE\nX/B PAUSE",
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
        .info_text = "DESCRIPTION\nFast arcade air combat.\nYour cannon fires automatically.\n\nGOAL\nDefeat "
                     "each wave and the boss.\n\nCONTROLS\nJOY MOVE\nA BOMB\nX/B PAUSE",
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
        .info_text =
            "DESCRIPTION\nStack seven kinds of blocks.\nComplete rows to clear them.\n\nGOAL\nClear lines "
            "and score high.\n\nCONTROLS\nJOY MOVE\nJOY DOWN SOFT DROP\nA ROTATE\nY HARD DROP\nX/B PAUSE",
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
        .info_text = "DESCRIPTION\nBounce the ball with a paddle.\nBreak every brick above.\n\nGOAL\nClear "
                     "the wall without a miss.\n\nCONTROLS\nJOY MOVE PADDLE\nA LAUNCH\nX/B PAUSE",
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
        .info_text =
            "DESCRIPTION\nClassic paddle duel against AI.\nReturn the ball past your rival.\n\nGOAL\nReach "
            "the winning score first.\n\nCONTROLS\nJOY MOVE PADDLE\nA SERVE\nX/B PAUSE",
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
        .info_text = "DESCRIPTION\nPlay Gomoku against the CPU.\nBuild a line on the board.\n\nGOAL\nConnect "
                     "five stones first.\n\nCONTROLS\nJOY MOVE CURSOR\nA PLACE\nX/B PAUSE",
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
        .info_text = "DESCRIPTION\nSlide and merge matching tiles.\nEach move creates a new "
                     "tile.\n\nGOAL\nBuild the 2048 tile.\n\nCONTROLS\nJOY SLIDE\nX/B PAUSE",
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
        .info_text =
            "DESCRIPTION\nRun through an endless desert.\nDodge ground and air hazards.\n\nGOAL\nSurvive and "
            "travel farther.\n\nCONTROLS\nA JUMP\nY DUCK OR FAST DROP\nX/B PAUSE",
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
        .info_text = "DESCRIPTION\nFly between narrow pipe gaps.\nGlide briefly when ready.\n\nGOAL\nPass as "
                     "many pipes as possible.\n\nCONTROLS\nA FLAP\nY GLIDE\nX/B PAUSE",
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
        .info_text = "DESCRIPTION\nExplore a changing maze.\nCollect gems along the route.\n\nGOAL\nFind the "
                     "marked exit quickly.\n\nCONTROLS\nJOY MOVE\nX/B PAUSE",
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
        .info_text =
            "DESCRIPTION\nFire needles at a spinning disk.\nDo not strike another needle.\n\nGOAL\nPlace "
            "every needle safely.\n\nCONTROLS\nJOY AIM\nA LAUNCH\nY QUICK STICK\nX/B PAUSE",
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
        .info_text = "DESCRIPTION\nMove inside a hostile arena.\nPatterns become more "
                     "dangerous.\n\nGOAL\nSurvive every attack pattern.\n\nCONTROLS\nJOY MOVE\nX/B PAUSE",
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
        .info_text =
            "DESCRIPTION\nMatch arrows to falling notes.\nKeep your timing and combo.\n\nGOAL\nFinish with "
            "the highest score.\n\nCONTROLS\nJOY MATCH ARROWS\nA START\nX/B PAUSE",
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
        .info_text = "DESCRIPTION\nBrowse the sound effect library.\nPreview each built-in "
                     "sound.\n\nGOAL\nFind and test an effect.\n\nCONTROLS\nJOY SELECT\nA PLAY\nB BACK",
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
        .info_text =
            "DESCRIPTION\nA compact four-function tool.\nEnter numbers and operators.\n\nGOAL\nCalculate a "
            "numeric result.\n\nCONTROLS\nJOY MOVE CURSOR\nA PRESS KEY\nB BACK",
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
        .info_text =
            "DESCRIPTION\nMeasure display frame speed.\nThe test runs a color workload.\n\nGOAL\nRead the "
            "measured FPS result.\n\nCONTROLS\nA START OR RESTART\nB BACK",
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
        .info_text =
            "DESCRIPTION\nAdjust the console sound level.\nChanges apply immediately.\n\nGOAL\nChoose a "
            "comfortable volume.\n\nCONTROLS\nJOY ADJUST\nA MUTE OR UNMUTE\nB BACK",
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
        .info_text = "DESCRIPTION\nView project and team pages.\nBrowse the built-in gallery.\n\nGOAL\nRead "
                     "each information page.\n\nCONTROLS\nJOY LEFT OR RIGHT\nB BACK",
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
