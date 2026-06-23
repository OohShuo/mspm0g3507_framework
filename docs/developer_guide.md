# 开发者指南

本文档说明在项目中添加功能、外设和游戏时应遵守的基本流程。

## 开发流程建议

1. 先确认功能属于 APP、HAL、BSP、LIB、VM 还是 TEST。
2. 优先在 VM 目标中验证应用逻辑。
3. 再构建 ARM 目标检查编译、链接、Flash/RAM 占用。
4. 上板测试外设行为、时序和真实输入输出。
5. 最后补充文档和测试开关。

## 添加新游戏

推荐步骤：

1. 在 `src/app/games/<game_name>/` 下创建 `.c/.h` 文件。
2. 实现以下接口：

```c
void Xxx_Init(const Game_hardware* hardware);
Game_result Xxx_Update(const Game_input* input);
uint32_t Xxx_Get_Score(void);
uint8_t Xxx_Is_Finished(void);
```

3. 在 `src/app/game_console/game_registry.h` 中添加 `Game_id` 和 `Game_icon`。
4. 在 `src/app/game_console/game_registry.c` 中加入 `Game_descriptor`。
5. 如需图标，在 `game_console.c` 中添加绘制函数并接入 `draw_grid_cell()`。
6. 在 VM 中验证输入、暂停、返回、结算和 FPS。

### 游戏输入规范

游戏应使用 `Game_input`：

- `axis_x` / `axis_y`：连续摇杆轴值。
- `direction` / `direction_pressed`：离散方向。
- `a_pressed`、`b_pressed`、`x_pressed`、`y_pressed`：ABXY 按键边沿。
- `confirm_pressed`：兼容确认输入，目前映射到 A。
- `back_requested`：兼容返回输入，目前映射到 B。

不要在游戏中直接读取 GPIO、ADC 或 SDL 键盘。

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

应用级开关放入 `config/app_config.h`，测试级开关放入 `config/test_config.h`。

建议：

- 构建开关控制是否编译/链接大型模块。
- 应用开关控制是否创建任务或启用功能。
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

项目根目录有 `.clang-format`，建议修改后执行格式化。通用建议：

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
