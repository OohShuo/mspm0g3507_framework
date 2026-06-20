# 游戏底部操作提示设计

## 目标

在游戏处于非暂停状态时，底部状态栏除 `X/B PAUSE` 外，还显示当前游戏的 A 键操作；仅当游戏确实使用 Y 键时显示 Y 键操作。暂停状态继续显示 `A/X RESUME B MENU`，非游戏应用继续使用原有提示。

## 设计

在 `Game_descriptor` 中增加静态 `control_hint` 字段，由游戏注册表保存每个游戏的简短英文操作提示。控制台通过一个统一辅助函数取得底栏文本，不在启动、恢复、重玩、屏保恢复等路径中分别维护文案。

非暂停游戏底栏由 `control_hint` 与 `X/B PAUSE` 组成，例如：

- Tank：`A FIRE  X/B PAUSE`
- Tetris：`A ROTATE Y DROP  X/B PAUSE`
- Dino：`A JUMP Y DUCK  X/B PAUSE`
- Flappy：`A FLAP Y GLIDE  X/B PAUSE`
- Needle：`A LAUNCH Y QUICK  X/B PAUSE`

完整映射如下：

| 游戏 | 操作提示 |
| --- | --- |
| PAC-MAN | 无 A/Y 运行操作 |
| SNAKE | 无 A/Y 运行操作 |
| TANK | `A FIRE` |
| AIR FORCE | `A BOMB` |
| TETRIS | `A ROTATE Y DROP` |
| BREAKOUT | `A LAUNCH` |
| PONG | `A SERVE` |
| GOMOKU | `A PLACE` |
| 2048 | 无 A/Y 运行操作 |
| DINO | `A JUMP Y DUCK` |
| FLAPPY | `A FLAP Y GLIDE` |
| MAZE | 无 A/Y 运行操作 |
| NEEDLE | `A LAUNCH Y QUICK` |
| DODGE | 无 A/Y 运行操作 |
| RHYTHM | `A START` |

没有 Y 专用逻辑时不显示 Y 或占位符。A 仅在准备/发球阶段有效的游戏仍显示该动作，因为它是玩家进入玩法所需的操作。若 A/Y 在游戏运行状态中没有作用，则不伪造提示，只显示暂停提示。

提示必须控制在底栏 FPS 区域左侧的可用宽度内。统一辅助函数负责返回最终文案，所有绘制入口都调用它：首次进入游戏、暂停后恢复、游戏结束后重玩，以及屏保退出后的界面恢复。

## 状态行为

- 运行中：显示当前游戏的 A/Y 操作及 `X/B PAUSE`。
- 暂停中：显示 `A/X RESUME B MENU`，行为和现状一致。
- 非游戏应用：保持 `A OK  B BACK`，不纳入本次游戏提示配置。
- 游戏结束界面：继续由现有 Game Over 菜单接管，不修改其提示。

## 测试与验证

- 单元测试验证每个 `is_game` 描述符都能生成正确的非暂停提示。
- 验证使用 Y 的游戏包含 Y 提示，不使用 Y 的游戏不包含 Y。
- 验证最终提示不会侵入 FPS 显示区域。
- 验证暂停提示和非游戏应用提示没有回归。
- 构建固件，确保描述符结构变化已覆盖所有注册项。

## 非目标

- 不改变任何按键行为或游戏规则。
- 不为不同游戏内部状态提供动态提示。
- 不调整暂停菜单、Game Over 菜单或底栏视觉样式。
