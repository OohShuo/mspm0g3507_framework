# VM 仿真器

VM 目标用于在 PC 上运行项目应用逻辑。它不需要 MSPM0G3507 硬件，适合调试菜单、游戏输入、界面刷新、音效和振动反馈策略。

## 构建与运行

```bash
python3 scripts/cc.py --target vm
./build/vm/framework_vm
```

## 键盘映射

| 键盘 | 框架输入 |
| --- | --- |
| 方向键 | 摇杆 X/Y 轴 |
| `S` | A |
| `D` | B |
| `W` | X |
| `A` | Y |
| `Space` | START / 摇杆按键兼容输入 |
| `Esc` | 退出 VM |

## VM 组成

| 文件/目录 | 作用 |
| --- | --- |
| `platform_vm.c` | SDL 初始化、主事件循环、虚拟任务启动和退出清理 |
| `input_vm.c` | 读取键盘状态并提供线程安全快照 |
| `display_vm.c` | 模拟 LCD 显示输出 |
| `audio_synth_vm.c` | 合成蜂鸣器声音 |
| `haptics_vm.c` | 模拟振动反馈 |
| `freertos/freertos_stubs.c` | 用 pthread/SDL ticks 模拟常用 FreeRTOS API |
| `hal/*_vm.c` | HAL 对象在 VM 中的适配实现 |
| `bsp/*.c` | BSP 接口在 VM 中的虚拟实现 |
| `lfs.h` + `lfs_vm_impl.c` | LittleFS API 的宿主机文件系统实现，直接读写 `assets/vm_flash/` 目录 |
| `storage_vm.c` | VM 存储层，`Storage_Get_Lfs()` 返回宿主机文件系统句柄 |

## 文件存储

VM 将 LittleFS 的所有文件操作（`lfs_file_open` / `lfs_file_read` / `lfs_file_write` 等）直接映射到宿主机文件系统，根目录为 `assets/vm_flash/`：

```text
assets/vm_flash/
  ├── ...
```

## 与 ARM 的差异

VM 目标追求“应用层行为一致”，不是完整硬件仿真：

- tick 来自 `SDL_GetTicks()`。
- 任务由 pthread 启动，不是真实 FreeRTOS 调度器。
- 部分队列/中断相关 API 只是占位或最小实现。
- LCD、蜂鸣器、振动和 Flash 都是软件模拟。
- 文件操作直接使用宿主机文件系统。

因此，VM 适合验证交互和应用逻辑；真实时序、外设电气特性、DMA、中断优先级仍需要在 ARM 硬件上验证。

## 推荐使用方式

1. 新游戏先在 VM 中完成基本逻辑和 UI。
2. 输入映射先使用 `Game_input`，不要在游戏内部直接读取 SDL 或 GPIO。
3. 需要硬件反馈时使用 `Game_hardware` 中的 `buzzer` 和 `vib_motor`。
4. VM 中能跑通后，再构建 ARM 目标检查资源占用和硬件行为。
