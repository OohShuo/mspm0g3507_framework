# Low Knight 移植计划

目标是把 `low.p8` 移植成 MSPM0G3507 上的独立游戏固件。当前芯片内部
Flash 只有 128 KiB，因此 Low Knight 不接入现有多游戏菜单，而是使用独立
固件入口，并把 PICO-8 资源放到外部 W25Q32。

## 存储方案

- MCU 内部 Flash：C 代码、少量常量、启动和驱动。
- W25Q32 高 2 MiB LittleFS：`/low_knight.p8r` 资源包。
- W25Q32 低 2 MiB raw 区：后续可保留给地图缓存或大图块缓存。

`/low_knight.p8r` 由 `scripts/pack_low_knight.py` 从 `low.p8` 生成，包含：

| 区段 | 大小 | 来源 |
| --- | ---: | --- |
| header | 24 B | 资源包头 |
| gfx | 8192 B | `__gfx__` |
| gff | 256 B | `__gff__` |
| map | 4096 B | `__map__` |

## 固件模式

在 `config/app_config.h` 中：

```c
#define LOW_KNIGHT_STANDALONE_ENABLE 1
#define FLASH_MGR_ENABLE 0
#define GAME_CONSOLE_ENABLE 1
```

`LOW_KNIGHT_STANDALONE_ENABLE=1` 时，`App_Task_Def()` 只创建 Low Knight
任务，不进入普通 Game Console。

上传资源时临时切换为：

```c
#define LOW_KNIGHT_STANDALONE_ENABLE 0
#define FLASH_MGR_ENABLE 1
#define GAME_CONSOLE_ENABLE 0
```

## 当前阶段

已完成第一阶段骨架：

1. 生成 `/low_knight.p8r` 资源包。
2. Low Knight 独立任务初始化 LCD、摇杆、按键、蜂鸣器。
3. 运行时从 LittleFS 打开资源包。
4. 屏幕显示资源状态，并从外部 Flash 读取 `gfx` 画出 tile 预览。
5. 解压起始房间的 RLE map 数据，绘制真实 tile 地图。
6. 在起始房间中绘制玩家 sprite，并用摇杆移动玩家占位。

下一阶段开始逐步移植 PICO-8 运行时语义：坐标、输入、地图读取、sprite 绘制、
实体系统、碰撞与关卡逻辑。
