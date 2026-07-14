#pragma once

#if FRAMEWORK_RUNTIME_MODE_CURRENT == FRAMEWORK_RUNTIME_MODE_FLASH_MGR
    #define FLASH_MGR_ENABLE 1
#else
    #define FLASH_MGR_ENABLE 0
#endif

#if FRAMEWORK_RUNTIME_MODE_CURRENT == FRAMEWORK_RUNTIME_MODE_GAME
    #define GAME_CONSOLE_ENABLE 1
#else
    #define GAME_CONSOLE_ENABLE 0
#endif

#define GAME_RUNTIME_MONITOR_ENABLE      0
#define GAME_RUNTIME_MONITOR_INTERVAL_MS 5000u

#define JOYSTICK_X_MIN_VOLTAGE           0.0f
#define JOYSTICK_X_MAX_VOLTAGE           3.3f
#define JOYSTICK_Y_MIN_VOLTAGE           0.0f
#define JOYSTICK_Y_MAX_VOLTAGE           3.3f

#define JOYSTICK_X_OFFSET                0.0f
#define JOYSTICK_Y_OFFSET                0.0f
#define JOYSTICK_X_REVERSE               0
#define JOYSTICK_Y_REVERSE               1

#define JOYSTICK_X_DEAD_ZONE             0.18f
#define JOYSTICK_Y_DEAD_ZONE             0.18f
#define JOYSTICK_DIRECTION_THRESHOLD     0.42f
#define JOYSTICK_DIRECTION_HYSTERESIS    0.05f

#define JOYSTICK_CALIBRATION_SAMPLES     32u
#define JOYSTICK_CALIBRATION_INTERVAL_MS 5u

#define LCD_MIRROR_X                     1
#define LCD_MIRROR_Y                     1
#define LCD_COLOR_USE_BGR                0

#if (GAME_CONSOLE_ENABLE || FLASH_MGR_ENABLE) && !FRAMEWORK_USE_FREERTOS
    #error "game and flash_mgr runtime modes require FRAMEWORK_USE_FREERTOS=ON"
#endif

#if FLASH_MGR_ENABLE && !FRAMEWORK_USE_LFS
    #error "flash_mgr runtime mode requires FRAMEWORK_USE_LFS=ON"
#endif

#if FLASH_MGR_ENABLE && !FRAMEWORK_USE_UART
    #error "flash_mgr runtime mode requires FRAMEWORK_USE_UART=ON"
#endif
