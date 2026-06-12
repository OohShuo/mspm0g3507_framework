# Lib — 库层

第三方库和项目内通用组件，通过 `lib/` 目录组织，各库通过 CMake 独立构建。

## 库列表

### FreeRTOS

`lib/freertos/` — FreeRTOS v11.x 实时操作系统（ARM Cortex-M0+ 移植）。

- 配置文件：`config/FreeRTOSConfig.h`
- 内存管理：`heap_4.c`（支持碎片合并）
- 控制宏：`FRAMEWORK_USE_FREERTOS`（`config/config.yaml`）

### LVGL

`lib/lvgl/` — LVGL v9.5 嵌入式图形库。

- 配置文件：`config/lvgl_config.h`
- 控制宏：`FRAMEWORK_USE_LVGL`
- 依赖 FreeRTOS（tick 和 mutex）

### LittleFS

`lib/lfs/` — LittleFS 嵌入式文件系统。

- 控制宏：`FRAMEWORK_USE_LFS`
- 通过 `lfs_port`（`src/app/lfs_port/`）适配 W25Q32 Flash 块设备

### RTT

`lib/RTT/` — SEGGER RTT（Real-Time Transfer），通过 SWD 调试口输出日志。

- 控制宏：`FRAMEWORK_USE_RTT`
- 文件：`SEGGER_RTT.c` / `SEGGER_RTT_printf.c`

### local_lib

`lib/local_lib/` — 项目内通用工具库。

| 子模块 | 说明 |
| --- | --- |
| `vector/` | 动态数组（泛型，支持 push/pop/insert/remove/qsort） |
| `protocol/` | 串口协议层：`protocol_none`（无协议）、`protocol_7d7e`（7D/7E 转义，类 SLIP）、`protocol_binary`（SYNC+LEN+CRC16） |
| `soft_crc/` | 软件 CRC16 计算 |
| `general_data/` | 通用数据管理 |
| `freertos_alloc/` | FreeRTOS 堆分配封装 |
| `bool_compat/` | `bool` 类型兼容（`stdbool.h` shim） |
| `lvgl_stubs.c` | LVGL 的 FreeRTOS tick/mutex 桩实现 |

### Wiznet

W5500 以太网芯片的 wiznet 官方驱动库，由 `src/hal/w5500/` 适配。

- 控制宏：`FRAMEWORK_USE_WIZNET`

## 系统入口

`lib/local_lib/local_lib.h`:

```c
void Local_Lib_Init(void);  // 初始化 local_lib（Vector、Protocol 等）
```
