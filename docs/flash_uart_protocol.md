# Flash UART 透传协议

基于二进制帧的串口 Flash 文件管理协议，用于 PC 远程管理 MPU 下挂的 W25Q32 外部 Flash 文件系统 (LittleFS)。

---

## 1. 物理层

| 参数 | 值 |
|------|-----|
| 接口 | UART |
| 波特率 | 115200 bps |
| 数据位 | 8 |
| 校验位 | None |
| 停止位 | 1 |
| 流控 | None |

---

## 2. 分层架构

```
┌──────────────────────────────────────────────────┐
│  flash_mgr.c               (应用层)              │
│  CMD 分发、LFS 操作、SEQ 去重                    │
│  on_chunk 收到: [CMD][SEQ][payload]              │
├──────────────────────────────────────────────────┤
│  com_uart.c                 (传输抽象)           │
│  DMA 收发、空闲中断、Protocol 生命周期           │
├──────────────────────────────────────────────────┤
│  protocol_binary.c          (帧协议)             │
│  SYNC 定界 + LEN 前缀 + CRC16 校验               │
│  recv_feed ← raw bytes                           │
│  send_pack → framed bytes                        │
└──────────────────────────────────────────────────┘
```

**protocol 模块** (`lib/local_lib/protocol/`) 提供三种传输编码，各自独立 `.c` 文件，通过 ops vtable 切换：

| 文件 | 类型 | 帧格式 |
|------|------|--------|
| `protocol_none.c` | `protocol_none` | 直通，无帧封装 |
| `protocol_7d7e.c` | `protocol_7d7e` | SLIP: `0x7F` ... `0x7E`，字节转义 |
| `protocol_binary.c` | `protocol_binary_frame` | SYNC + LEN + CRC16（本协议使用） |

`protocol.c` 仅做 `Protocol_Create` / `Protocol_Destroy`，通过 `extern` 引用各 ops table。

---

## 3. 帧格式 (protocol_binary_frame)

``` plain
┌────────┬────────┬────────┬────────┬──────────────────────────────────┬──────────┐
│ SYNC0  │ SYNC1  │ LEN_H  │ LEN_L  │         PAYLOAD (LEN 字节)       │  CRC16   │
│ 0xAA   │ 0x55   │ 1 Byte │ 1 Byte │         CMD+SEQ+DATA             │  2 Byte  │
├────────┴────────┴────────┴────────┴──────────────────────────────────┴──────────┤
│                               CRC16 覆盖区域                                     │
│                     LEN(2) + PAYLOAD(LEN)  =  2 + LEN 字节                       │
└──────────────────────────────────────────────────────────────────────────────────┘
```

| 字段 | 偏移 | 大小 | 说明 |
|------|------|------|------|
| SYNC0 | 0 | 1 | 帧同步头 0xAA |
| SYNC1 | 1 | 1 | 帧同步头 0x55 |
| LEN | 2 | 2 | Payload 长度，大端序。最小 3（仅 CMD+SEQ） |
| PAYLOAD | 4 | 3~515 | CMD(1) + SEQ(2) + DATA(0~512) |
| CRC16 | 4+LEN | 2 | CRC16，大端序 |

**固定开销**: 6 字节。**最大帧长**: 521 字节。

### 3.1 帧定界

通过 **同步字 + 长度字段 + CRC 校验** 三重定界，不使用转义：

1. 搜索连续字节 `0xAA 0x55` 定位帧头（`0xAA 0xAA 0x55` 视为合法前导）
2. 读取 2 字节 LEN
3. 读取 LEN 字节 PAYLOAD
4. 读取 2 字节 CRC16，校验通过 → 触发 `on_chunk` 回调

CRC 校验失败时**静默丢弃帧**（不发送 CRC_ERR 响应），发送方通过超时机制自动重传。

### 3.2 接收状态机

位于 `protocol_binary.c:bin_recv_feed`：

```
SYNC0 → SYNC1 → LEN(2B) → DATA(LEN B) → CRC(2B)
  ↑        ↓(非0xAA/0x55)                  │
  └────────┴────────────────────────────────┘
              CRC 失败：静默丢弃，回到 SYNC0
              CRC 通过 → on_chunk(PAYLOAD)
```

### 3.3 CRC16 规范

| 参数 | 值 |
|------|-----|
| 实现 | `lib/local_lib/soft_crc` (`Soft_Crc16_Calc`) |
| 多项式 | 0x1021 |
| 初始值 | 0xFFFF |
| 算法 | 反射查表法（reflected table-based） |
| 覆盖范围 | LEN(2) + PAYLOAD(LEN 字节) |
| 测试向量 | `CRC16(b"123456789") = 0x6F91` |

PC 端使用等价的查表实现，见 `scripts/flash_manager.py:crc16()`。

---

## 4. 指令集

### 4.1 主机请求 (PC → MCU)

| 码值 | 助记符 | 说明 | PAYLOAD 中 DATA 格式 |
|------|--------|------|---------------------|
| 0x01 | READ | 读取文件 | `[path_len:1B][path:N][offset:4B LE]` |
| 0x02 | WRITE | 写文件块 | `[path_len:1B][path:N][offset:4B LE][data:M]` |
| 0x03 | DELETE | 删除文件 | `[path_len:1B][path:N]` |
| 0x04 | LIST | 列出目录 | `[path_len:1B][path:N]` (空=根目录) |
| 0x05 | INFO | 文件信息 | `[path_len:1B][path:N]` |
| 0x06 | FORMAT | 格式化 FS | 无 |
| 0x07 | RESET | 软复位 | 无 |

### 4.2 设备响应 (MCU → PC)

| 码值 | 助记符 | 说明 | PAYLOAD 中 DATA 格式 |
|------|--------|------|---------------------|
| 0x80 | ACK | 操作成功 | 无 |
| 0x81 | NAK | 操作失败 | `[err_code:1B]` |
| 0x82 | — | 保留（原 CRC_ERR，协议层静默丢弃坏帧，不再发送） | — |
| 0x83 | BUSY | Flash 忙 | 无 |
| 0x84 | CHUNK | 文件数据块 | `[offset:4B LE][data:M]` |
| 0x85 | EOF | 文件传输完毕 | `[file_size:4B LE]` |
| 0x86 | LIST_ITEM | 目录项 | `[type:1B][name_len:1B][name:N][size:4B LE]` |
| 0x87 | LIST_END | 目录列举完毕 | `[count:2B LE]` |
| 0x88 | INFO_RESP | 文件信息响应 | `[type:1B][size:4B LE]` |

### 4.3 错误码 (NAK.data[0])

| 码值 | 含义 |
|------|------|
| 0x00 | 未知错误 |
| 0x01 | 文件未找到 (ENOENT) |
| 0x02 | Flash 空间不足 (ENOSPC) |
| 0x03 | 路径格式错误 |
| 0x04 | 文件已存在 |
| 0x05 | I/O 错误 |
| 0x06 | 文件系统损坏 |

---

## 5. 通信流程

### 5.1 读文件 (READ)

```
PC                                    MCU
 │                                      │
 │─ READ seq=0, path, offset=0 ───────→│ ① 请求从偏移 0 开始读
 │                                      │   lfs_file_open + lfs_file_seek + lfs_file_read
 │←─ CHUNK seq=0, offset=0, data ──────│ ② 返回数据块 (≤512B)
 │                                      │
 │─ READ seq=1, path, offset=512 ─────→│ ③ 请求下一块
 │←─ CHUNK seq=1, offset=512, data ────│
 │                                      │
 │─ READ seq=2, path, offset=1024 ────→│
 │←─ EOF seq=2, file_size ─────────────│ ④ 文件读完
```

**断点续传**: 若 CHUNK 丢失（帧 CRC 坏 → 协议层静默丢弃 → PC 超时），PC 以相同 offset 重发 READ，完全幂等。

### 5.2 写文件 (WRITE)

```
PC                                    MCU
 │                                      │
 │─ WRITE seq=0, path, offset=0, ─────→│ ① 写第一块
 │   data(512B)                         │   lfs_file_open + lfs_file_seek +
 │                                      │   lfs_file_write + lfs_file_close
 │←─ ACK seq=0 ────────────────────────│
 │                                      │
 │─ WRITE seq=1, path, offset=512 ────→│ ② 写第二块
 │←─ ACK seq=1 ────────────────────────│
 │                                      │
 │─ WRITE seq=2, path, offset=1024, ──→│ ③ 最后一块
 │   data(200B)                         │
 │←─ ACK seq=2 ────────────────────────│
```

**幂等性**: 每个 WRITE 帧独立 open → seek → write → close。重传同一 offset+data 无副作用。

### 5.3 删除 / 列表 / 格式化

```
DELETE:  PC → DELETE seq=N, path → MCU → ACK seq=N
                                    MCU → NAK seq=N (err=0x01)

LIST:    PC → LIST seq=N, path     → MCU → LIST_ITEM ... ×N
                                    MCU → LIST_END

FORMAT:  PC → FORMAT seq=N         → MCU → ACK seq=N
```

---

## 6. 错误处理

### 6.1 超时重传

| 参数 | 值 | 说明 |
|------|-----|------|
| PC_RETRY_MAX | 3 | 最大重试次数 |
| PC_TIMEOUT | 500 ms | 等待响应超时 |
| MCU_TIMEOUT | 2000 ms | Flash 操作超时 |

```
PC 发送帧
  ├─ 收到 ACK/CHUNK/EOF → 成功
  ├─ 收到 BUSY → 等 100ms 重试
  ├─ 收到 NAK → 报告错误
  └─ 超时 → 重传（最多 3 次）
```

### 6.2 重复帧

MCU 记住最后成功处理的 SEQ。收到相同 SEQ 的帧时：
- WRITE: 丢弃数据，重发 ACK（幂等性保证安全）
- READ/LIST: 丢弃，重发对应响应
- 其他: 丢弃，重发 ACK

### 6.3 BUSY 响应

Flash 正在执行耗时操作（如擦除）时，MCU 发送 BUSY。PC 等待 100ms 后重试。

---

## 7. 内存约束 (MCU 端)

Cortex-M0+ / 32 KB SRAM。所有大缓冲区用 static (BSS) 或 FreeRTOS heap：

| 缓冲区 | 大小 | 位置 | 所属模块 |
|--------|------|------|----------|
| 帧解析临时缓冲 | 520 B | static (BSS) | protocol_binary.c |
| 响应组装缓冲 `g_tx_buf` | 530 B | static (BSS) | flash_mgr.c |
| TX 编码缓冲 `Protocol.tx_buf` | 521 B | FreeRTOS heap | protocol.c |
| RX 解码缓冲 `Protocol.rx_buf` | 515 B | FreeRTOS heap | protocol.c |
| 命令队列 `g_cmd_queue` | 4 × 517 B | FreeRTOS heap | flash_mgr.c |
| CHUNK 响应缓冲 | 516 B | static (BSS) | flash_mgr.c |
| LIST_ITEM 缓冲 | 262 B | static (BSS) | flash_mgr.c |

禁止在栈上分配大于 64 字节的缓冲区。

---

## 8. 路径规范

- 最大长度: 255 字节
- 编码: UTF-8
- 格式: `/dir/subdir/file.txt`
- 根目录: `/` 或空字符串
- 必须以 `/` 开头，不支持 `..` 和 `.`

---

## 9. 文件类型

| type 值 | 含义 |
|---------|------|
| 0x00 | 未知 |
| 0x01 | 普通文件 (LFS_TYPE_REG) |
| 0x02 | 目录 (LFS_TYPE_DIR) |

---

## 10. 代码索引

| 文件 | 角色 |
|------|------|
| `lib/local_lib/protocol/protocol.h` | Protocol 类型枚举、结构体、ops vtable |
| `lib/local_lib/protocol/protocol.c` | `Protocol_Create` / `Protocol_Destroy`，extern ops |
| `lib/local_lib/protocol/protocol_none.c` | 直通编码实现 |
| `lib/local_lib/protocol/protocol_7d7e.c` | SLIP 帧编码实现 |
| `lib/local_lib/protocol/protocol_binary.c` | 二进制帧编码实现 |
| `lib/local_lib/soft_crc/soft_crc.c` | CRC16 查表算法 |
| `src/hal/com_uart/com_uart.c` | UART DMA 收发 + Protocol 集成 |
| `src/app/flash_mgr/flash_mgr.h` | 协议常量、`Flash_mgr_cmd` 结构体 |
| `src/app/flash_mgr/flash_mgr.c` | 帧回调、CMD 分发、LFS 操作、任务创建 |
| `src/app/lfs_port/lfs_port.c` | LittleFS 块设备驱动 + SPI mutex |
| `scripts/flash_manager.py` | Python 客户端 |

---

> 文档版本: 2.0 | 更新日期: 2026-06-12
