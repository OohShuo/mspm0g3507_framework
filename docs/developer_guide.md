# 开发者指南

本文档说明在项目中添加功能、外设和游戏时应遵守的基本流程。

## 开发流程建议

1. 先确认功能属于 APP、HAL、BSP、LIB、VM 还是 TEST。
2. 优先在 VM 目标中验证应用逻辑。
3. 再构建 ARM 目标检查编译、链接、Flash/RAM 占用。
4. 上板测试外设行为、时序和真实输入输出。
5. 最后补充文档和测试开关。

## VM 资源文件

VM 目标直接使用 `assets/vm_flash/` 目录下的文件，无需构建闪存镜像：

- 添加新资源文件（`.r565`、`.bard`、`.tune` 等）只需放入该目录。
- 运行时生成的文件（如 `scores.bin`）也会保存在该目录。
- 如果需要在 ARM 上使用，再通过 `flash_manager.py` 上传到 W25Q32。

## 添加新游戏

推荐步骤：

1. 创建 `src/app/games/<token>.c`，在文件内实现静态回调并导出唯一的 `game_<token>_entry` 描述符。无需创建普通游戏头文件。如需图片/贴图等资源文件，放入 `src/app/game_assets/`。
2. 回调形式如下：

```c
static void xxx_init(const Game_hardware* hardware);
static Game_result xxx_update(const Game_input* input);
static uint32_t xxx_get_score(void);
static void xxx_draw_icon(St7789* lcd, int32_t x, int32_t y);
```

`xxx_update()` 在正常运行时返回 `game_result_running`，请求退出时返回 `game_result_exit`，并在进入终态的当帧立即返回 `game_result_won` 或 `game_result_lost`。不要在游戏内显示结局/重玩提示或播放通用胜负反馈；控制台统一处理结算、排行榜和重玩。非游戏工具只使用 `running` 与 `exit`。

3. 在 `src/app/game_console/game_entries.inc` 的目标菜单位置添加 `game_entry(<token>)`。该顺序同时生成连续 `Game_id` 和注册数组顺序，不需要手改 `game_registry.c`。
4. 描述符中的 `draw_icon` 使用 48x40 局部画布，`name_color` 指定未选中标题颜色。
5. 在 VM 中验证输入、暂停、返回、结算和 FPS。调整既有条目顺序时还需考虑排行榜 ID 兼容迁移。

### 游戏输入规范

游戏应使用 `Game_input`：

- `axis_x` / `axis_y`：连续摇杆轴值。
- `direction` / `direction_pressed`：离散方向。
- `a_pressed`、`b_pressed`、`x_pressed`、`y_pressed`：ABXY 按键边沿。
- `confirm_pressed`：兼容确认输入，目前映射到 A。
- `back_requested`：兼容返回输入，目前映射到 B。

不要在游戏中直接读取 GPIO、ADC 或 SDL 键盘。

### 游戏随机数规范

需要随机数时，在游戏私有状态中保存独立的 `Game_rng`：

```c
static Game_rng g_rng;

Game_Rng_Seed(&g_rng, Game_Runtime_Get_Tick_Ms() ^ GAME_SEED_CONSTANT);
uint32_t choice = Game_Rng_Range(&g_rng, upper_bound);
```

每个游戏使用不同的非零 `GAME_SEED_CONSTANT`。有界选择必须使用 `Game_Rng_Range()`；不要自行实现 LCG 或用 `% upper_bound` 缩减随机值。

## 添加新 HAL 设备

1. 在 `src/hal/<device>/` 中创建模块。
2. 提供 `Init`、`Create`、`Update` 或必要操作接口。
3. HAL 对象保存设备状态，底层动作调用 BSP。
4. 在 `src/hal/hal.c` 中加入初始化和周期更新。
5. 在 `src/vm/hal/` 中提供 VM 版本或 stub。
6. 在 `src/test/` 中添加测试任务，并在 `config/test_config.h` 中添加开关。

## 添加新 BSP 外设

1. 在 `src/bsp/<peripheral>/` 中封装最小外设操作。
2. 将引脚、索引、通道等配置写入 `config/board_config.h` 或 SysConfig。
3. 不在 BSP 中写业务逻辑。
4. 如 ARM 使用 SysConfig 生成对象，确认 `config/framework.syscfg` 和 `config/syscfg/` 同步。
5. 为 VM 添加对应虚拟实现，避免破坏 `--target vm`。

## 添加配置开关

构建级开关放入 `config/config.yaml`：

```yaml
FRAMEWORK_USE_UART: ON
```

顶层运行模式也在 `config/config.yaml` 中选择：

```yaml
runtime_mode: game # game | flash_mgr | test
```

游戏机和 Flash Manager 的任务开关由该字段自动派生，不直接编辑 `config/app_config.h`。测试级开关仍放在 `config/test_config.h`；选择 `runtime_mode: test` 后，可手动启用一个或多个 `TEST_*_ENABLE`。

建议：

- 构建开关控制是否编译/链接大型模块。
- `runtime_mode` 控制创建哪个顶层应用任务。
- 测试开关只影响测试任务，不应改变正式逻辑。

## 调试资源占用

ARM 构建后优先查看：

```text
build/arm/framework.map
```

常用分析方向：

- 哪些 `.o` 文件占用最多 Flash。
- 大数组、图片资源、字体资源是否进入 `.rodata`。
- 静态缓冲区是否占用 `.bss` / `.data`。
- `--gc-sections` 是否成功去掉未使用函数。

## 代码风格

项目根目录有 `.clang-format`，建议修改后执行格式化：

```bash
source scripts/format.bash
```

通用建议：

- 模块接口放 `.h`，实现放 `.c`。
- 对外函数使用模块名前缀，例如 `Vib_Motor_Gpio_*`、`Vib_Motor_Pwm_*`、`Game_Console_*`。
- 不把平台相关代码混入 APP。
- 不在中断或短周期任务中做复杂绘制和阻塞操作。
- 新增大型资源前先评估 Flash/RAM。

## 文档维护

文档位于 `docs/`，README 与 `docs/index.md` 保持一致。修改入口说明后，记得同步：

```bash
cp README.md docs/index.md
```

预览：

```bash
bash scripts/serve_docs.sh
```
