# 04 — Middleware

## FreeRTOS

Kernel v11.x, Cortex-M0+ port.

### Key Config

| Param | Value | Why |
| --- | --- | --- |
| `configTICK_RATE_HZ` | 1000 | 1ms resolution |
| `configMAX_PRIORITIES` | 5 | Timer(4) > FlashMgr(2) > App(1) > Idle(0) |
| `configTOTAL_HEAP_SIZE` | 18KB | heap_4 best-fit |
| `configUSE_PREEMPTION` | 1 | Flash Manager preempts games |
| `configUSE_MUTEXES` | 1 | Storage mutex |
| `configUSE_TASK_NOTIFICATIONS` | 1 | Lightweight signaling |
| `configCHECK_FOR_STACK_OVERFLOW` | 2 | Canary check |

### Objects Used

| Object | Where | Purpose |
| --- | --- | --- |
| Mutex | Storage | Serialize Flash SPI access |
| Queue | Flash Manager | UART command buffering (depth 4) |
| xTaskCreate | hal.c, app.c | All task creation |

### ADR

Why FreeRTOS vs RTX/CMSIS-RTOS2/Zephyr/bare-metal: [adr/architecture_decisions.md §2](adr/architecture_decisions.md#2-freertos).

## LittleFS

Fail-safe 文件系统，挂载于 W25Q32 高 2 MiB。Copy-on-write 元数据，动态磨损均衡。

### Block Device

W25Q32 sector=4KB, page=256B. LFS block_size=4096, read_size=256, prog_size=256.

### LFS Port

```c
typedef struct {
    W25q32* flash;
    uint32_t start;   // 2 MiB offset
    uint32_t size;    // 2 MiB region
    SemaphoreHandle_t spi_mutex;
} Lfs_port_config;
Lfs_port* Lfs_Port_Create(config);
int Lfs_Port_Mount/Format/Unmount(port);
lfs_t* Lfs_Port_Get_Lfs(port);
```

Static buffers (528B total): read 256B, prog 256B, lookahead 16B。不从 heap 分配，避免碎片化。

### ADR

Why LittleFS vs FatFS/SPIFFS/raw: [adr/architecture_decisions.md §3](adr/architecture_decisions.md#3-littlefs).

## LVGL

v9.5，**可选**（`FRAMEWORK_USE_LVGL=OFF`）。禁用时零资源消耗。

### Display Driver

ST7789 flush callback: `St7789_Flush → lv_disp_flush_ready`. RGB565, 240×320.

### Memory

- Flash: ~45KB (library + widgets)
- RAM: 单缓冲 ~15KB, 双缓冲 ~30KB
- 当前默认禁用：游戏使用直接 framebuffer 渲染，节省 RAM

### ADR

Why optional vs always-on/custom: [adr/architecture_decisions.md §4](adr/architecture_decisions.md#4-lvgl).

## SEGGER RTT

printf → RTT Channel 0 (J-Link SWD). 无需 UART 引脚。

### Why Macro, Not newlib Retarget

newlib-nano + nosys → `_isatty` returns -1 → stdout fully buffered → short lines lost.  
`#define printf(...) SEGGER_RTT_printf(0, __VA_ARGS__)` 绕过所有 FILE buffer.

```c
// rtt_log.h
#if FRAMEWORK_USE_RTT
    #define printf(...) SEGGER_RTT_printf(0, __VA_ARGS__)
#else
    #define printf(...) ((void)0)  // zero-cost when disabled
#endif
```

### Config

Up-Buffer 0: 1024B, Down-Buffer 0: 16B. Host: J-Link RTT Viewer.
