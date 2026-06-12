# WIZnet W5500 — UDP 通信

W5500 硬件 TCP/IP 芯片，SPI 接口。8 个 socket 可独立配置为 TCP/UDP/MACRAW。以下只涉及 UDP。

## 1. 硬件连接

```
W5500        MCU (MSPM0)
─────        ──────────
SCLK   ←──   SPI1_SCK  (PA17)   共享 SPI 总线
MOSI   ←──   SPI1_PICO (PB8)
MISO   ──→   SPI1_POCI (PB9)
SCSn   ←──   任意 GPIO (CS)
RSTn   ←──   任意 GPIO (可选)
```

SPI Mode 0 (CPOL=0, CPHA=0)，MSB first，≤33 MHz。与 LCD/W25Q32 共享总线，通过 CS 脚区分。

## 2. 文件

```
lib/wiznet/Ethernet/
├── wizchip_conf.c/h    # 芯片配置，SPI 回调注册
├── socket.c/h          # BSD socket API
└── W5500/w5500.c/h     # W5500 寄存器

src/hal/w5500/
├── w5500_hal.h         # SPI/CS/临界区回调（底层，通常不直接使用）
└── w5500_hal.c

src/hal/com_udp/        # ← 应用层使用这个
├── com_udp.h           # 与 com_uart 相同接口模式
└── com_udp.c           # 内部管理 W5500 + socket + polling
```

## 3. 编译

```bash
cmake ... -DFRAMEWORK_USE_WIZNET=ON -DWIZNET_W5500_ENABLED=ON
```

CMakeLists.txt 中 `FRAMEWORK_USE_WIZNET` 控制 wiznet 库是否编译。其他芯片型号和上层协议（DHCP/DNS/MQTT 等）全部关闭，只编译 `Ethernet/` 核心。

## 4. 使用 — `com_udp` 接口

接口与 `com_uart` 对齐：`Init` → `Create` → `Poll` / `Send`，通过 config 的 `on_rx` 回调接收数据。

### 4.1 初始化

```c
#include "com_udp.h"

static Com_udp* g_udp = NULL;

static void on_udp_rx(Com_udp* obj, const uint8_t* data, uint32_t len,
                       uint8_t flags, void* arg) {
    uint8_t  src_ip[4];
    uint16_t src_port;
    Com_Udp_Get_Src(obj, src_ip, &src_port);
    printf("rx %lu B from %d.%d.%d.%d:%d\n",
           (unsigned long)len, src_ip[0],src_ip[1],src_ip[2],src_ip[3], src_port);
    Com_Udp_Send(obj, data, len, src_ip, src_port);  // echo
}

void udp_init(void) {
    Com_Udp_Init();

    // W5500 实例 — 类似 W25q32_Create
    Wiz5500* wiz = W5500_Create(&(W5500_Config){
        .spi_idx = 0, .cs_gpio_idx = 10, .rst_gpio_idx = 11,
        .spi_mutex = NULL,
    });

    Com_udp_config cfg = {
        .wiz = wiz,                              // ← 传 Wiz5500 实例指针
        .mac = {0x02,0,0,0,0,1},
        .ip  = {192,168,1,100},
        .sn  = {255,255,255,0},
        .gw  = {192,168,1,1},
        .sock_n = 0, .local_port = 8888,
        .rx_buf_size = 256, .tx_buf_size = 256,
        .on_rx = on_udp_rx, .on_rx_arg = NULL,
    };
    g_udp = Com_Udp_Create(&cfg);
}
```

### 4.2 FreeRTOS 轮询

```c
static void task_udp(void* arg) {
    udp_init();
    uint32_t tick = xTaskGetTickCount();
    while (1) {
        Com_Udp_Poll();
        vTaskDelayUntil(&tick, pdMS_TO_TICKS(10));
    }
}
xTaskCreate(task_udp, "UDP", 512, NULL, 1, NULL);
```

### 4.3 主动发送

```c
uint8_t dest[] = {192,168,1,50};
Com_Udp_Send(g_udp, (uint8_t*)"ping", 4, dest, 9999);
```

### 4.4 实例关系

```
Wiz5500* wiz = W5500_Create(...);      // 一个硬件实例（单例）
Com_udp* s0 = Com_Udp_Create({.wiz=wiz, .sock_n=0, ...});  // socket 0
Com_udp* s1 = Com_Udp_Create({.wiz=wiz, .sock_n=1, ...});  // socket 1
// Com_Udp_Poll() 自动驱动全部
```

### 4.5 com_uart vs com_udp 对照

| 操作 | com_uart | com_udp |
|------|----------|---------|
| 硬件实例 | （内建） | `W5500_Create(&cfg)` → `Wiz5500*` |
| 初始化 | `Com_Uart_Init()` | `Com_Udp_Init()` |
| 创建 | `Com_Uart_Create(&cfg)` | `Com_Udp_Create(&cfg)` ← cfg.wiz |
| 发送 | `Com_Uart_Send(obj, data, len)` | `Com_Udp_Send(obj, data, len, ip, port)` |
| 接收 | 自动（ISR） | `Com_Udp_Poll()` → on_rx |
| 源地址 | — | `Com_Udp_Get_Src(obj, ip, port)` |

## 5. API 速查

| 函数 | 说明 |
|------|------|
| `Com_Udp_Init()` | 子系统初始化（幂等） |
| `Com_Udp_Create(&cfg)` | 创建 socket，返回 Com_udp* |
| `Com_Udp_Poll()` | 轮询所有实例，有数据时调 on_rx |
| `Com_Udp_Send(obj, data, len, ip, port)` | 发送 UDP 报文 |
| `Com_Udp_Get_Src(obj, out_ip, out_port)` | 获取最后收到的源地址 |

## 6. SPI 共享与内存

W5500 / W25Q32 / ST7789 共用 SPI1。`w5500_hal.c` 自动 take/give mutex。

| 项目 | 大小 | 位置 |
|------|------|------|
| RX/TX payload 缓冲 | 各 256 B | FreeRTOS heap (Com_Udp_Create) |
| W5500 socket 缓冲 | 2 KB per socket | 片内（不占 MCU SRAM） |
| SPI DMA 暂存 | 512 B | BSS (bsp_hard_spi) |

---

> Datasheet: https://docs.wiznet.io/Product/iEthernet/W5500/datasheet
