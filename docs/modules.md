# 模块设计

本文档按目录介绍项目主要模块。

## APP：应用与游戏层

目录：`src/app`

APP 层负责用户可见功能，包括游戏控制台、游戏、存储、资源管理等。入口在 `App_Init()` 和 `App_Task_Def()`：

- `App_Init()`：在启用 LittleFS 时初始化存储系统。
- `App_Task_Def()`：根据开关创建 Flash 管理任务和游戏控制台任务。

主要子模块：

| 子模块 | 说明 |
| --- | --- |
| `game_console` | 菜单、游戏信息页、暂停、结束菜单、屏保、分数显示、统一输入分发 |
| `games` | PAC-MAN、Snake、Tank、Air Force、Tetris、Breakout、Pong、Gomoku、2048、Dino、Flappy、Maze、Needle、Calculator、Info 等 |
| `storage` / `lfs_port` | LittleFS 接入与应用存储接口 |
| `flash_mgr` | 外部 Flash 管理逻辑 |
| `image_asset` | 图片资源访问封装 |

### 游戏注册接口

每个游戏或工具在自己的主 `.c` 中导出一个 `Game_descriptor`。`game_entries.inc` 以同一顺序生成连续 ID、外部描述符声明和注册指针数组：

```c
typedef struct {
    const char* name;
    Game_id id;
    const char* control_hint;
    const char* info_text;
    Game_draw_icon draw_icon;
    uint16_t name_color;
    void (*init)(const Game_hardware* hardware);
    Game_result (*update)(const Game_input* input);
    uint32_t (*get_score)(void);
    uint8_t is_game;
} Game_descriptor;
```

新增条目只需创建 `src/app/games/<token>.c`（生命周期和图标回调均为 `static`），再在 `game_entries.inc` 添加 `game_entry(<token>)`。排行榜存储版本 4 使用该连续 ID 顺序；加载版本 1 或 3 数据时会迁移旧 ID，并丢弃已移除的槽位。

`update` 直接报告生命周期结果：

```c
typedef enum {
    game_result_running,
    game_result_exit,
    game_result_won,
    game_result_lost,
} Game_result;
```

游戏在进入终态的当帧返回 `won` 或 `lost`；工具只返回 `running` 或 `exit`。控制台统一负责终态音效与振动、分数录入、排行榜和重玩，因此不需要轮询游戏内部状态。

需要随机数的游戏在自身私有状态中保存 `Game_rng`，通过运行时接口操作：

```c
void Game_Rng_Seed(Game_rng* rng, uint32_t seed);
uint32_t Game_Rng_Next(Game_rng* rng);
uint32_t Game_Rng_Range(Game_rng* rng, uint32_t upper_bound);
```

初始化时使用 `Game_Runtime_Get_Tick_Ms()` 与游戏专属非零常量异或后播种；有界随机选择使用无偏的 `Game_Rng_Range()`，不要在游戏中定义私有 PRNG。

## HAL：设备对象层

目录：`src/hal`

HAL 把硬件能力封装成对象，供 APP 使用。典型对象包括：

| 模块 | 作用 |
| --- | --- |
| `button` | GPIO 按键、消抖、上下状态读取 |
| `joystick` | ADC 摇杆、中心校准、死区、归一化轴值 |
| `st7789` | LCD 初始化、窗口设置、像素/矩形/图像绘制 |
| `w25q32` | 外部 SPI Flash 读写擦除 |
| `buzzer` | PWM 蜂鸣器、音效播放 |
| `vib_motor_pwm/gpio` | 振动马达、效果模式、优先级与冷却 |
| `led_simple` / `led_breath` | 普通 LED 和呼吸灯 |
| `com_uart` | UART 通信封装，可由开关启用 |

HAL 的设计原则是“对象持有状态，BSP 执行动作”。例如 GPIO 振动马达保存播放模式、优先级和时间状态，最终输出通过 `Bsp_Gpio_Write()` 驱动控制脚；PWM 版本则通过 `Bsp_Pwm_*` 完成。

## BSP：板级外设层

目录：`src/bsp`

BSP 直接封装 MCU 外设能力：

| 模块 | 说明 |
| --- | --- |
| `gpio` | GPIO 输入输出 |
| `pwm` | PWM 频率、占空比、启动停止 |
| `adc` | ADC 采样，摇杆使用 |
| `spi` | 硬件 SPI 与软件 SPI |
| `uart` | UART、DMA、空闲中断相关封装 |
| `time` | 系统 tick / ms 时间 |

BSP 编译为静态库 `bsp`。ARM 目标中，BSP 会链接 TI DriverLib、SysConfig 生成文件和 FreeRTOS。

## LIB：中间件与通用库

目录：`lib`

| 模块 | 说明 |
| --- | --- |
| `freertos` | ARM 目标使用的 FreeRTOS 内核和 Cortex-M0 移植层 |
| `lfs` | LittleFS 文件系统 |
| `lvgl` | 可选图形库 |
| `RTT` | 可选 SEGGER RTT 日志 |
| `local_lib` | CRC、vector、协议封装、FreeRTOS 内存辅助、LVGL stub 等 |

`lib/CMakeLists.txt` 根据 `FRAMEWORK_USE_*` 开关决定实际加入哪些库。

## PLATFORM：平台适配层

目录：`src/platform`

平台层负责区分 ARM 和 VM：

- ARM：执行 `SYSCFG_DL_init()`、启用中断、启动 FreeRTOS 调度器。
- VM：初始化 SDL2、虚拟显示、输入、振动，启动虚拟任务，并运行 SDL 事件循环。

CMake 中的 `platform_add_executable()` 也在这里定义：ARM 生成 `framework.elf`，VM 生成 `framework_vm`。

## VM：桌面仿真层

目录：`src/vm`

VM 用 SDL2 提供虚拟设备：

- `display_vm`：模拟 ST7789 显示。
- `input_vm`：把键盘映射为摇杆和 A/B/X/Y/START。
- `audio_synth_vm`：模拟蜂鸣器声音。
- `haptics_vm`：模拟振动反馈。
- `freertos`：用 pthread/SDL ticks 模拟项目使用的 FreeRTOS API。
- `hal` / `bsp`：为 VM 提供同名虚拟实现。

## TEST：测试层

目录：`src/test`

测试模块由 `config/test_config.h` 控制。每个测试项都有单独开关，例如：

```c
#define TEST_BUTTON_ENABLE         0
#define TEST_VIB_MOTOR_GPIO_ENABLE 0
#define TEST_VIB_MOTOR_PWM_ENABLE  0
#define TEST_W25Q32_ENABLE         0
```

当 `config/config.yaml` 选择 `runtime_mode: test`，且任意测试开启使 `TEST_ANY_ENABLE` 为真时，`main.c` 才会调用 `Test_Task_Def()` 创建对应测试任务。

## CONFIG：配置层

目录：`config`

主要文件：

| 文件 | 说明 |
| --- | --- |
| `config.yaml` | 构建目标、顶层运行模式与功能开关 |
| `board_config.h` | GPIO/PWM/ADC/SPI/UART 索引、引脚、摇杆校准、LCD 方向等 |
| `app_config.h` | 应用层功能开关 |
| `test_config.h` | 测试任务开关 |
| `FreeRTOSConfig.h` | FreeRTOS 配置 |
| `framework.syscfg` | TI SysConfig 工程文件 |
