# App — 应用层

应用层位于 HAL/BSP/库层之上，实现具体的业务逻辑。任务通过 `App_Task_Def()` 统一注册。

## 系统入口

`src/app/app.h`

| API | 说明 |
| --- | --- |
| `App_Init()` | 应用层初始化 |
| `App_Task_Def()` | 创建所有应用 FreeRTOS 任务 |

`App_Task_Def()` 在 `main()` 中按顺序调用，当前创建的任务：

1. **FlashMgr** — Flash Manager 任务（受 `FLASH_MGR_ENABLE` 控制）

## flash_mgr — Flash 远程管理

`src/app/flash_mgr/flash_mgr.h`

通过 UART 协议提供远程 Flash 文件系统管理功能。上位机通过二进制帧协议读写、删除、列举 W25Q32 上 LittleFS 分区中的文件。

### 控制宏

```c
#define FLASH_MGR_ENABLE 0  // 0 = 禁用（Init/Loop 为空函数），1 = 启用
```

依赖 `FRAMEWORK_USE_LFS=ON`。

### API

| API | 说明 |
| --- | --- |
| `Flash_Mgr_Init()` | 初始化 W25Q32、lfs_port、SPI mutex、命令队列、Com UART |
| `Flash_Mgr_Loop(void* arg)` | FreeRTOS 任务入口，阻塞等待命令队列并分发处理 |

任务配置：栈 1024 words，优先级 1。

### 协议概览

- 物理层：UART（`FLASH_MGR_UART_IDX`），空闲超时 5ms
- 帧协议：`protocol_binary_frame`（SYNC AA 55 + 长度 + CRC16）
- 支持命令：READ / WRITE / DELETE / LIST / INFO / FORMAT / RESET
- 详细协议说明见 `docs/flash_uart_protocol.md`

## lfs_port — LittleFS 块设备适配

`src/app/lfs_port/lfs_port.h`

将 W25Q32 Flash 适配为 LittleFS 的块设备后端。受 `FRAMEWORK_USE_LFS` 控制。

| API | 说明 |
| --- | --- |
| `Lfs_Port_Create(cfg)` | 创建 lfs_port 实例 |
| `Lfs_Port_Mount(obj)` | 挂载文件系统 |
| `Lfs_Port_Unmount(obj)` | 卸载文件系统 |
| `Lfs_Port_Format(obj)` | 格式化（创建空 superblock） |
| `Lfs_Port_Get_Lfs(obj)` | 获取底层 `lfs_t*` 句柄 |

配置字段：`flash`（W25q32 句柄）、`start`（分区起始偏移）、`size`（分区大小）、`spi_mutex`（SPI 互斥锁，可选）。

Flash 布局：W25Q32 高 2 MiB 划给 lfs 分区，低 2 MiB 留给固件/资源。
