# Low Knight 验证步骤

## 生成资源包

```powershell
python scripts/pack_low_knight.py low.p8 build/low_knight.p8r
```

期望输出：

- `gfx: 8192 bytes`
- `gff: 256 bytes`
- `map: 4096 bytes`
- 总文件大小为 12568 bytes

## 上传资源包

先烧录上传固件：

```c
#define LOW_KNIGHT_STANDALONE_ENABLE 0
#define FLASH_MGR_ENABLE 1
#define GAME_CONSOLE_ENABLE 0
```

然后上传：

```powershell
python scripts/flash_manager.py COM6 upload build/low_knight.p8r /low_knight.p8r
python scripts/flash_manager.py COM6 info /low_knight.p8r
```

## 运行独立固件

切回 Low Knight 固件：

```c
#define LOW_KNIGHT_STANDALONE_ENABLE 1
#define FLASH_MGR_ENABLE 0
#define GAME_CONSOLE_ENABLE 1
```

重新构建烧录。屏幕应显示：

- 起始房间地图
- 玩家 sprite 位于房间中部偏下
- 摇杆可移动玩家占位

如果资源读取失败，会停留在 `RESOURCE MISS` 或资源状态页面。

如果显示 `RESOURCE MISS`，说明 LittleFS 中还没有 `/low_knight.p8r`，或资源包头
不匹配。
