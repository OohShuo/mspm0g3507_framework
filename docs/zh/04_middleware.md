# 04 — 中间件

## FreeRTOS

内核 v11.x，Cortex-M0+ 移植。

### 关键配置

| 参数 | 值 | 原因 |
| --- | --- | --- |
| `configTICK_RATE_HZ` | 1000 | 1ms 分辨率 |
| `configMAX_PRIORITIES` | 5 | Timer(4) > FlashMgr(2) > App(1) > Idle(0) |
| `configTOTAL_HEAP_SIZE` | 14KB | heap_4 best-fit 算法 |
| `configUSE_PREEMPTION` | 1 | Flash Manager 可抢占游戏 |
| `configUSE_MUTEXES` | 1 | 存储互斥锁 |
| `configUSE_TASK_NOTIFICATIONS` | 1 | 轻量级任务通知 |
| `configCHECK_FOR_STACK_OVERFLOW` | 2 | 金丝雀检测 |

### 使用的对象

| 对象 | 位置 | 用途 |
| --- | --- | --- |
| 互斥锁 | 存储 | 串行化 Flash SPI 访问 |
| 队列 | Flash Manager | UART 命令缓冲（深度 4） |
| xTaskCreate | hal.c、app.c | 所有任务创建 |

### ADR

为何选 FreeRTOS 而非 RTX/CMSIS-RTOS2/Zephyr/裸机：[../en/adr/architecture_decisions.md §2](../en/adr/architecture_decisions.md#2-freertos)。

## LittleFS

防掉电文件系统，挂载于 W25Q32 高 2 MiB。写时复制元数据，动态磨损均衡。

### 块设备

W25Q32 扇区=4KB，页=256B。LFS block_size=4096，read_size=256，prog_size=256。

### LFS Port

```c
typedef struct {
    W25q32* flash;
    uint32_t start;   // 2 MiB 偏移
    uint32_t size;    // 2 MiB 区域
    SemaphoreHandle_t spi_mutex;
} Lfs_port_config;
Lfs_port* Lfs_Port_Create(config);
int Lfs_Port_Mount/Format/Unmount(port);
lfs_t* Lfs_Port_Get_Lfs(port);
```

静态缓冲区（共 528B）：读 256B、写 256B、预读 16B。不从堆分配，避免碎片化。

### ADR

为何选 LittleFS 而非 FatFS/SPIFFS/裸二进：[../en/adr/architecture_decisions.md §3](../en/adr/architecture_decisions.md#3-littlefs)。

## LVGL

v9.5，**可选**（`FRAMEWORK_USE_LVGL=OFF`）。禁用时资源消耗可忽略（编译期排除）。

### 显示驱动

ST7789 flush 回调：`St7789_Flush → lv_disp_flush_ready`。RGB565，240×320。

### 内存

- Flash：约 45KB（库 + 控件）
- RAM：单缓冲约 15KB，双缓冲约 30KB
- 当前默认禁用：游戏使用直接 framebuffer 渲染，节省 RAM

### ADR

为何可选而非强制/自定义：[../en/adr/architecture_decisions.md §4](../en/adr/architecture_decisions.md#4-lvgl)。

## SEGGER RTT

printf → RTT Channel 0（J-Link SWD）。无需 UART 引脚。

### 为何用宏而非 newlib 重定向

newlib-nano + nosys → `_isatty` 返回 -1 → stdout 全缓冲 → 短日志行丢失。  
`#define printf(...) SEGGER_RTT_printf(0, __VA_ARGS__)` 绕过所有 FILE 缓冲区。

```c
// rtt_log.h
#if FRAMEWORK_USE_RTT
    #define printf(...) SEGGER_RTT_printf(0, __VA_ARGS__)
#else
    #define printf(...) ((void)0)  // 禁用时几乎零开销
#endif
```

### 配置

上行缓冲区 0：1024B，下行缓冲区 0：16B。主机端：J-Link RTT Viewer。
