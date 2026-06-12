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

## 2. 帧格式

```
┌────────┬────────┬────────┬────────┬────────┬────────┬───────────┬──────────┐
│ SYNC0  │ SYNC1  │  CMD   │ SEQ_H  │ SEQ_L  │ LEN_H  │  LEN_L    │  DATA    │
│ 0xAA   │ 0x55   │ 1 Byte │ 1 Byte │ 1 Byte │ 1 Byte │  1 Byte   │ 0 ~ 512B │
├────────┴────────┴────────┴────────┴────────┴────────┴───────────┴──────────┤
│                              CRC16 覆盖区域                                 │
├────────────────────────────────────────────────────────────────────────────┤
│  CRCH   │  CRCL   │
│ 1 Byte  │ 1 Byte  │
└─────────┴─────────┘
```

| 字段 | 偏移 | 大小 | 说明 |
|------|------|------|------|
| SYNC0 | 0 | 1 | 帧同步头 0xAA |
| SYNC1 | 1 | 1 | 帧同步头 0x55 |
| CMD | 2 | 1 | 指令/响应码 |
| SEQ | 3 | 2 | 序列号，大端序 (Big-Endian) |
| LEN | 5 | 2 | 数据长度，大端序。0 表示无负载 |
| DATA | 7 | 0~512 | 负载数据 |
| CRC16 | 7+LEN | 2 | CCITT CRC16，大端序 |

**固定开销**: 9 字节。**最大帧长**: 521 字节。

### 2.1 帧定界

不使用 SLIP/COBS 转义。通过**同步字 + 长度字段 + CRC 校验**三重保障定界：

1. 搜索连续字节 `0xAA 0x55` 定位帧头
2. 读取固定头 (CMD+SEQ+LEN = 6 字节)
3. 根据 LEN 读取 DATA
4. 读取 2 字节 CRC16 并校验

若 CRC 校验失败，接收方回复 `CRC_ERR` 响应，发送方重传。

### 2.2 CRC16 规范

使用项目内置 `lib/local_lib/soft_crc` 库（`Soft_Crc16_Calc`）。Python 端使用等价的查表实现。

| 参数 | 值 |
|------|-----|
| 多项式 | 0x1021 |
| 初始值 | 0xFFFF |
| 算法 | 反射查表法（reflected table-based） |
| 覆盖范围 | CMD + SEQ + LEN + DATA（不含 SYNC 和 CRC 自身） |

> **注意**: 此实现是 bit-reversed 反射算法，与标准的 CRC-16/CCITT-FALSE (0x29B1) 不同。两端均使用相同的 `soft_crc` 算法，互操作不受影响。测试向量: `CRC16(b"123456789") = 0x6F91`。

---

## 3. 指令集

### 3.1 主机请求 (PC → MCU)

| 码值 | 助记符 | 说明 | DATA 格式 |
|------|--------|------|-----------|
| 0x01 | READ | 读取文件 | `[path_len:1B][path:N][offset:4B LE]` |
| 0x02 | WRITE | 写文件块 | `[path_len:1B][path:N][offset:4B LE][data:M]` |
| 0x03 | DELETE | 删除文件 | `[path_len:1B][path:N]` |
| 0x04 | LIST | 列出目录 | `[path_len:1B][path:N]` (空=根目录) |
| 0x05 | INFO | 文件信息 | `[path_len:1B][path:N]` |
| 0x06 | FORMAT | 格式化 FS | 无 |
| 0x07 | RESET | 复位设备 | 无 |

### 3.2 设备响应 (MCU → PC)

| 码值 | 助记符 | 说明 | DATA 格式 |
|------|--------|------|-----------|
| 0x80 | ACK | 操作成功 | 无 |
| 0x81 | NAK | 操作失败 | `[err_code:1B]` |
| 0x82 | CRC_ERR | CRC 校验失败 | 无 |
| 0x83 | BUSY | Flash 忙 | 无 |
| 0x84 | CHUNK | 文件数据块 | `[offset:4B LE][data:M]` |
| 0x85 | EOF | 文件传输完毕 | `[file_size:4B LE]` |
| 0x86 | LIST_ITEM | 目录项 | `[type:1B][name_len:1B][name:N][size:4B LE]` |
| 0x87 | LIST_END | 目录列举完毕 | `[count:2B LE]` |
| 0x88 | INFO_RESP | 文件信息响应 | `[type:1B][size:4B LE]` |

### 3.3 错误码 (NAK.data[0])

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

## 4. 通信流程

### 4.1 读文件 (READ)

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

**断点续传**: 若 CHUNK 丢失，PC 超时后以相同 offset 重发 READ，操作幂等。

**文件末尾判定**: MCU 返回的 CHUNK 长度 < 512 或返回 EOF 表示文件结束。

### 4.2 写文件 (WRITE)

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
 │─ WRITE seq=2, path, offset=1024, ──→│ ③ 最后一块 (<512B 表示文件尾)
 │   data(200B)                         │
 │←─ ACK seq=2 ────────────────────────│
```

**幂等性**: 每个 WRITE 帧独立完成 open → seek → write → close。重传相同 offset+data 不会产生副作用。

**断点续传**: PC 维护 `last_acked_offset`。若超时，从最后确认的 offset 继续发送。

### 4.3 删除文件 (DELETE)

```
PC:  DELETE seq=N, path → MCU: ACK seq=N
                          MCU: NAK seq=N, err=0x01 (文件不存在)
```

### 4.4 目录列表 (LIST)

```
PC:  LIST seq=0, path="/data" → MCU: LIST_ITEM seq=0, name="log.txt", type=FILE
                                MCU: LIST_ITEM seq=0, name="cfg", type=DIR
                                MCU: LIST_END seq=0, count=2
```

### 4.5 格式化 (FORMAT)

```
PC:  FORMAT seq=0 → MCU: ACK seq=0
```

---

## 5. 错误处理

### 5.1 CRC 错误

```
接收方 → CRC_ERR(seq) → 发送方: 重传原帧（相同 seq）
```

### 5.2 超时重传

| 参数 | 值 | 说明 |
|------|-----|------|
| PC_RETRY_MAX | 3 | 最大重试次数 |
| PC_TIMEOUT | 500 ms | 等待响应超时 |
| MCU_TIMEOUT | 2000 ms | Flash 操作超时(含擦除等待) |

```
PC 发送帧 → 启动定时器
  ├─ 收到 ACK → 成功，继续下一帧
  ├─ 收到 CRC_ERR → 重传
  └─ 超时 → 重传（最多 3 次，超过则报错）
```

### 5.3 重复帧

接收方记住最后成功处理的 SEQ。若收到相同 SEQ 的帧：
- **WRITE**: 丢弃数据，重发 ACK（利用幂等性）
- **READ/LIST**: 丢弃，重发对应响应
- **其他**: 丢弃，重发 ACK

### 5.4 BUSY 响应

当 Flash 正在执行耗时操作（如擦除）时，MCU 可发送 BUSY 响应。PC 收到 BUSY 后等待 100ms 再重试。

---

## 6. 接收状态机 (MCU 端)

```
                    ┌────────────────────┐
                    │    HUNT_SYNC0       │
                    │  等待 0xAA          │
                    └──────┬─────────────┘
                           │ got 0xAA
                    ┌──────▼─────────────┐
                    │    HUNT_SYNC1       │
                    │  等待 0x55          │
                    └──────┬─────────────┘
                           │ got 0x55
                    ┌──────▼─────────────┐
                    │     HEADER          │
                    │  读 6 字节固定头    │
                    └──────┬─────────────┘
                           │
                           ├── CMD 无效 → HUNT_SYNC0
                           ├── LEN > 512 → HUNT_SYNC0
                           │
                    ┌──────▼─────────────┐
                    │      DATA           │
                    │  读 LEN 字节        │
                    └──────┬─────────────┘
                           │
                    ┌──────▼─────────────┐
                    │      CRC            │
                    │  读 2 字节 + 校验   │
                    └──────┬─────────────┘
                           │
                    ┌──────▼─────────────┐
                    │     DISPATCH        │
                    │  CRC 通过 → 入队    │
                    │  CRC 失败 → CRC_ERR │
                    └────────────────────┘
                    然后返回 HUNT_SYNC0
```

---

## 7. 路径规范

- 最大长度: 255 字节
- 编码: UTF-8
- 格式: `/dir/subdir/file.txt`
- 根目录: `/` 或空字符串
- 必须以 `/` 开头
- 不支持 `..` 和 `.`

---

## 8. 文件类型

| type 值 | 含义 |
|---------|------|
| 0x00 | 未知 |
| 0x01 | 普通文件 (LFS_TYPE_REG) |
| 0x02 | 目录 (LFS_TYPE_DIR) |

---

## 9. 内存约束 (MCU 端)

| 缓冲区 | 大小 | 位置 |
|--------|------|------|
| 帧头缓冲 | 6 字节 | 栈 |
| DATA 缓冲 | 512 字节 | static (BSS) |
| 响应帧缓冲 | 530 字节 | static (BSS) |
| 命令队列 | 4 × 520 字节 | FreeRTOS Heap |

禁止在栈上分配大于 64 字节的缓冲区。

---

## 10. Python 端 CRC16 参考实现

```python
def crc16_ccitt(data: bytes) -> int:
    """CRC-16/CCITT-FALSE, 匹配 MCU 端 Soft_Crc16_Calc"""
    poly = 0x1021
    crc = 0xFFFF
    for byte in data:
        crc ^= (byte << 8)
        for _ in range(8):
            if crc & 0x8000:
                crc = (crc << 1) ^ poly
            else:
                crc = (crc << 1)
            crc &= 0xFFFF
    return crc
```

---

> 文档版本: 1.0 | 更新日期: 2026-06-12
