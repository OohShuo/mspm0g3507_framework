#if FRAMEWORK_USE_LFS

// clang-format off

#include "flash_mgr.h"

#include <stddef.h>
#include <string.h>

#include "board_config.h"
#include "bsp_uart.h"
#include "com_uart.h"
#include "freertos_alloc.h"
#include "lfs_port.h"
#include "lfs.h"
#include "rtt_log.h"
#include "w25q32.h"

/* ------------------------------------------------------------------ */
/* Flash region layout                                                  */
/*                                                                     */
/* Upper 2 MiB of the 4 MiB W25Q32 are dedicated to the managed lfs    */
/* partition.  The lower 2 MiB are left for assets, firmware, etc.     */
/* ------------------------------------------------------------------ */

#define FLASH_MGR_LFS_START  (2u * 1024u * 1024u)  /* 2 MiB             */
#define FLASH_MGR_LFS_SIZE   (2u * 1024u * 1024u)  /* 2 MiB             */

/* ------------------------------------------------------------------ */
/* UART / protocol config                                                */
/*                                                                     */
/* Uses protocol_binary_frame: SYNC+LEN+CRC framing provided by the    */
/* protocol layer.  flash_mgr only sees [CMD][SEQ][payload].           */
/* RX/TX buffers sized for max wire frame (~521 B).                    */
/* ------------------------------------------------------------------ */

#define FLASH_MGR_UART_IDX            0u
#define FLASH_MGR_RX_MAX_LEN          600u
#define FLASH_MGR_TX_MAX_LEN          600u
#define FLASH_MGR_IDLE_TIMEOUT_MS     5u
#define FLASH_MGR_PROTO_MAX_PAYLOAD   520u  /* CMD(1)+SEQ(2)+data(512)+offset(4) max = 519 */

/* ------------------------------------------------------------------ */
/* Task / queue config                                                  */
/* ------------------------------------------------------------------ */

#define FLASH_MGR_TASK_STACK_WORDS 1024
#define FLASH_MGR_TASK_PRIORITY    1u
#define FLASH_MGR_QUEUE_DEPTH      4u

/* ------------------------------------------------------------------ */
/* Static module state                                                  */
/* ------------------------------------------------------------------ */

static Com_uart*     g_com_uart     = NULL;
static W25q32*       g_flash        = NULL;
static Lfs_port*     g_lfs_port     = NULL;
static SemaphoreHandle_t g_spi_mutex = NULL;
static QueueHandle_t g_cmd_queue    = NULL;
static TaskHandle_t  g_task_handle  = NULL;

/* ---- Last-processed SEQ for duplicate detection ------------------- */

static uint16_t g_last_seq     = 0;
static uint8_t  g_has_last_seq = 0;

/* ---- Payload assembly buffer ---------------------------------------
 *
 * Com_Uart_Send → protocol_binary.send_pack wraps payload with
 * SYNC+LEN+CRC.  g_tx_buf holds the application payload
 * [CMD(1)][SEQ(2)][data(N)], max 3+512=515 B.                        */

static uint8_t g_tx_buf[FLASH_MGR_TX_BUF_SIZE];

/* ------------------------------------------------------------------ */
/* Forward declarations                                                 */
/* ------------------------------------------------------------------ */

static void flash_mgr_task(void* arg);
static void flash_mgr_on_rx(Com_uart* obj, const uint8_t* data, uint32_t len,
                             uint8_t flags, void* arg);
static void send_response(uint8_t cmd, uint16_t seq,
                          const uint8_t* data, uint16_t data_len);
static void send_ack(uint16_t seq);
static void send_nak(uint16_t seq, uint8_t err_code);

/* ---- Command handlers (called from flash_mgr_task) ---------------- */

static void handle_read(const Flash_mgr_cmd* cmd);
static void handle_write(const Flash_mgr_cmd* cmd);
static void handle_delete(const Flash_mgr_cmd* cmd);
static void handle_list(const Flash_mgr_cmd* cmd);
static void handle_info(const Flash_mgr_cmd* cmd);
static void handle_format(const Flash_mgr_cmd* cmd);

/* ------------------------------------------------------------------ */
/* Error mapping                                                        */
/* ------------------------------------------------------------------ */

static uint8_t map_lfs_error(int lfs_err) {
    switch (lfs_err) {
        case 0:                     return 0;
        case LFS_ERR_NOENT:         return FLASH_MGR_ERR_NOENT;
        case LFS_ERR_NOSPC:         return FLASH_MGR_ERR_NOSPC;
        case LFS_ERR_IO:            return FLASH_MGR_ERR_IO;
        case LFS_ERR_CORRUPT:       return FLASH_MGR_ERR_CORRUPT;
        case LFS_ERR_EXIST:         return FLASH_MGR_ERR_EXIST;
        case LFS_ERR_INVAL:         return FLASH_MGR_ERR_INVAL;
        default:                    return FLASH_MGR_ERR_UNKNOWN;
    }
}

static uint8_t lfs_type_to_mgr_type(uint8_t lfs_type) {
    if (lfs_type == LFS_TYPE_REG) { return FLASH_MGR_TYPE_REG; }
    if (lfs_type == LFS_TYPE_DIR) { return FLASH_MGR_TYPE_DIR; }
    return FLASH_MGR_TYPE_UNKNOWN;
}

/* ------------------------------------------------------------------ */
/* Response helpers (called from flash_mgr task)                        */
/*                                                                     */
/* Each helper assembles [CMD][SEQ][data] into g_tx_buf and calls      */
/* Com_Uart_Send, which invokes protocol_binary.send_pack to wrap it   */
/* with SYNC+LEN+CRC.                                                  */
/* ------------------------------------------------------------------ */

static void send_response(uint8_t cmd, uint16_t seq,
                          const uint8_t* data, uint16_t data_len) {
    if (g_com_uart == NULL) { return; }
    /* Wait for previous DMA to finish — essential for LIST where
     * multiple frames are sent back-to-back. */
    Bsp_Uart_Wait_For_Complete(g_com_uart->config.uart_idx);

    /* Assemble application payload: [CMD(1)][SEQ(2)][data(N)] */
    g_tx_buf[0] = cmd;
    g_tx_buf[1] = (uint8_t)(seq >> 8);
    g_tx_buf[2] = (uint8_t)(seq);
    if (data_len > 0 && data != NULL) {
        memcpy(g_tx_buf + 3, data, data_len);
    }
    /* protocol_binary.send_pack wraps g_tx_buf[0..3+data_len) */
    Com_Uart_Send(g_com_uart, g_tx_buf, (uint32_t)(3u + data_len));
}

static void send_ack(uint16_t seq) {
    send_response(FLASH_MGR_RESP_ACK, seq, NULL, 0);
}

static void send_nak(uint16_t seq, uint8_t err_code) {
    send_response(FLASH_MGR_RESP_NAK, seq, &err_code, 1);
}

/* ------------------------------------------------------------------ */
/* UART RX callback — frame parser state machine                        */
/*                                                                     */
/* Runs in the context of com_uart's idle ISR callback.  Must not       */
/* block or touch the Flash.  On a valid frame, copies the command to   */
/* the FreeRTOS queue; on CRC failure, sends CRC_ERR directly.          */
/* ------------------------------------------------------------------ */

/* ------------------------------------------------------------------ */
/* UART RX callback                                                     */
/*                                                                     */
/* Called by com_uart → protocol_binary.recv_feed once per complete,   */
/* CRC-verified frame.  data[] = [CMD(1)][SEQ(2)][payload(N)].         */
/* We validate, deduplicate, and queue to the flash_mgr task.          */
/* ------------------------------------------------------------------ */

static void flash_mgr_on_rx(Com_uart* obj, const uint8_t* data, uint32_t len,
                             uint8_t flags, void* arg) {
    (void)obj;
    (void)arg;
    (void)flags;  /* binary frames are always FIRST|LAST */

    /* Minimum: CMD(1) + SEQ(2) */
    if (len < 3) { return; }

    uint8_t  cmd = data[0];
    uint16_t seq = ((uint16_t)data[1] << 8) | data[2];
    uint16_t payload_len = (uint16_t)(len - 3u);

    /* Duplicate detection */
    if (g_has_last_seq && seq == g_last_seq) {
        send_ack(seq);
        return;
    }

    /* Validate command byte */
    if (cmd < FLASH_MGR_CMD_READ || cmd > FLASH_MGR_CMD_RESET) {
        send_nak(seq, FLASH_MGR_ERR_INVAL);
        return;
    }

    /* Queue to flash_mgr task */
    Flash_mgr_cmd queue_item;
    queue_item.cmd      = cmd;
    queue_item.seq      = seq;
    queue_item.data_len = payload_len;
    if (payload_len > 0) {
        memcpy(queue_item.data, data + 3, payload_len);
    }

    if (g_cmd_queue != NULL) {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        BaseType_t ok = xQueueSendToBackFromISR(g_cmd_queue, &queue_item, &xHigherPriorityTaskWoken);
        if (ok != pdPASS) {
            send_response(FLASH_MGR_RESP_BUSY, seq, NULL, 0);
        }
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }

    g_last_seq     = seq;
    g_has_last_seq = 1;
}

/* ------------------------------------------------------------------ */
/* Flash Manager task                                                   */
/* ------------------------------------------------------------------ */

static void flash_mgr_task(void* arg) {
    (void)arg;

    Flash_mgr_cmd cmd;

    while (1) {
        /* Block until a command arrives */
        if (xQueueReceive(g_cmd_queue, &cmd, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        switch (cmd.cmd) {
            case FLASH_MGR_CMD_READ:    handle_read(&cmd);    break;
            case FLASH_MGR_CMD_WRITE:   handle_write(&cmd);   break;
            case FLASH_MGR_CMD_DELETE:  handle_delete(&cmd);  break;
            case FLASH_MGR_CMD_LIST:    handle_list(&cmd);    break;
            case FLASH_MGR_CMD_INFO:    handle_info(&cmd);    break;
            case FLASH_MGR_CMD_FORMAT:  handle_format(&cmd);  break;
            case FLASH_MGR_CMD_RESET:
                /* Soft reset: just ACK; the host may follow with a
                 * hardware reset or the task can trigger a system reset. */
                send_ack(cmd.seq);
                break;
            default:
                send_nak(cmd.seq, FLASH_MGR_ERR_INVAL);
                break;
        }
    }
}

/* ------------------------------------------------------------------ */
/* Command handlers                                                     */
/* ------------------------------------------------------------------ */

/* ---- READ ------------------------------------------------------------
 *
 *   Request:  [path_len:1B][path:N][offset:4B LE]
 *   Response: CHUNK([offset:4B LE][data:M])  or  EOF([file_size:4B LE])
 *   Error:    NAK([err_code:1B])
 */

static void handle_read(const Flash_mgr_cmd* cmd) {
    lfs_t* lfs = Lfs_Port_Get_Lfs(g_lfs_port);
    if (lfs == NULL) { send_nak(cmd->seq, FLASH_MGR_ERR_IO); return; }

    /* Validate minimum payload: path_len(1) + offset(4) = 5 bytes */
    if (cmd->data_len < 5) { send_nak(cmd->seq, FLASH_MGR_ERR_INVAL); return; }

    uint8_t  path_len = cmd->data[0];
    uint32_t offset   = (uint32_t)cmd->data[1 + path_len]
                      | ((uint32_t)cmd->data[2 + path_len] << 8)
                      | ((uint32_t)cmd->data[3 + path_len] << 16)
                      | ((uint32_t)cmd->data[4 + path_len] << 24);

    if (path_len == 0 || path_len > FLASH_MGR_PATH_MAX ||
        (uint32_t)(1 + path_len + 4) > cmd->data_len) {
        send_nak(cmd->seq, FLASH_MGR_ERR_INVAL);
        return;
    }

    /* Extract null-terminated path */
    char path[FLASH_MGR_PATH_MAX + 1];
    memcpy(path, cmd->data + 1, path_len);
    path[path_len] = '\0';

    /* Take SPI mutex for the duration of the LFS op */
    if (g_spi_mutex != NULL) { xSemaphoreTake(g_spi_mutex, portMAX_DELAY); }

    lfs_file_t f;
    int err = lfs_file_open(lfs, &f, path, LFS_O_RDONLY);
    if (err < 0) {
        if (g_spi_mutex != NULL) { xSemaphoreGive(g_spi_mutex); }
        send_nak(cmd->seq, map_lfs_error(err));
        return;
    }

    /* Seek to requested offset */
    err = lfs_file_seek(lfs, &f, (lfs_soff_t)offset, LFS_SEEK_SET);
    if (err < 0) {
        lfs_file_close(lfs, &f);
        if (g_spi_mutex != NULL) { xSemaphoreGive(g_spi_mutex); }
        send_nak(cmd->seq, map_lfs_error(err));
        return;
    }

    /* Read up to CHUNK_SIZE bytes */
    static uint8_t chunk_buf[FLASH_MGR_CHUNK_SIZE];
    lfs_ssize_t nread = lfs_file_read(lfs, &f, chunk_buf, FLASH_MGR_CHUNK_SIZE);

    /* Get file size for EOF detection */
    lfs_soff_t file_size = lfs_file_size(lfs, &f);
    lfs_file_close(lfs, &f);

    if (g_spi_mutex != NULL) { xSemaphoreGive(g_spi_mutex); }

    if (nread < 0) {
        send_nak(cmd->seq, map_lfs_error((int)nread));
        return;
    }

    if (nread == 0) {
        /* Already at EOF (offset >= file_size) */
        uint8_t eof_data[4];
        eof_data[0] = (uint8_t)(file_size);
        eof_data[1] = (uint8_t)(file_size >> 8);
        eof_data[2] = (uint8_t)(file_size >> 16);
        eof_data[3] = (uint8_t)(file_size >> 24);
        send_response(FLASH_MGR_RESP_EOF, cmd->seq, eof_data, 4);
    } else {
        /* Build CHUNK: [offset:4B LE][data:nread].
         * Use the g_tx_buf scratch area that build_frame already writes
         * into — build_frame wants CMD+SEQ+LEN+DATA starting at offset 2,
         * so we can prep the DATA portion here and let send_response
         * wrap it.  Simpler: just memcpy chunk data directly after the
         * 4-byte offset into a static buffer.  (chunk_buf is static,
         * but it's only 512 B; we need 516 B for offset+data.) */
        static uint8_t chunk_resp[4 + FLASH_MGR_CHUNK_SIZE];
        chunk_resp[0] = (uint8_t)(offset);
        chunk_resp[1] = (uint8_t)(offset >> 8);
        chunk_resp[2] = (uint8_t)(offset >> 16);
        chunk_resp[3] = (uint8_t)(offset >> 24);
        memcpy(chunk_resp + 4, chunk_buf, (size_t)nread);

        send_response(FLASH_MGR_RESP_CHUNK, cmd->seq, chunk_resp,
                      (uint16_t)(4 + nread));
    }
}

/* ---- WRITE -----------------------------------------------------------
 *
 *   Request:  [path_len:1B][path:N][offset:4B LE][data:M]
 *   Response: ACK  or  NAK([err_code:1B])
 *
 *   Each WRITE frame is self-contained: open → seek → write → close.
 *   Retransmission of the same SEQ with the same offset+data is idempotent.
 */

static void handle_write(const Flash_mgr_cmd* cmd) {
    lfs_t* lfs = Lfs_Port_Get_Lfs(g_lfs_port);
    if (lfs == NULL) { send_nak(cmd->seq, FLASH_MGR_ERR_IO); return; }

    /* Minimum: path_len(1) + offset(4) = 5 bytes.  Data may be 0 bytes
     * (truncate at offset). */
    if (cmd->data_len < 5) { send_nak(cmd->seq, FLASH_MGR_ERR_INVAL); return; }

    uint8_t  path_len = cmd->data[0];

    if (path_len == 0 || path_len > FLASH_MGR_PATH_MAX ||
        (uint32_t)(1 + path_len + 4) > cmd->data_len) {
        send_nak(cmd->seq, FLASH_MGR_ERR_INVAL);
        return;
    }

    uint32_t offset = (uint32_t)cmd->data[1 + path_len]
                    | ((uint32_t)cmd->data[2 + path_len] << 8)
                    | ((uint32_t)cmd->data[3 + path_len] << 16)
                    | ((uint32_t)cmd->data[4 + path_len] << 24);

    uint16_t header_size = (uint16_t)(1 + path_len + 4);
    uint16_t chunk_len   = cmd->data_len - header_size;

    /* Extract path */
    char path[FLASH_MGR_PATH_MAX + 1];
    memcpy(path, cmd->data + 1, path_len);
    path[path_len] = '\0';

    const uint8_t* chunk_data = cmd->data + header_size;

    if (g_spi_mutex != NULL) { xSemaphoreTake(g_spi_mutex, portMAX_DELAY); }

    lfs_file_t f;
    int err = lfs_file_open(lfs, &f, path,
                            LFS_O_WRONLY | LFS_O_CREAT);
    if (err < 0) {
        if (g_spi_mutex != NULL) { xSemaphoreGive(g_spi_mutex); }
        send_nak(cmd->seq, map_lfs_error(err));
        return;
    }

    err = lfs_file_seek(lfs, &f, (lfs_soff_t)offset, LFS_SEEK_SET);
    if (err < 0) {
        lfs_file_close(lfs, &f);
        if (g_spi_mutex != NULL) { xSemaphoreGive(g_spi_mutex); }
        send_nak(cmd->seq, map_lfs_error(err));
        return;
    }

    if (chunk_len > 0) {
        lfs_ssize_t written = lfs_file_write(lfs, &f, chunk_data, chunk_len);
        if (written < 0) {
            lfs_file_close(lfs, &f);
            if (g_spi_mutex != NULL) { xSemaphoreGive(g_spi_mutex); }
            send_nak(cmd->seq, map_lfs_error((int)written));
            return;
        }
        if ((uint16_t)written != chunk_len) {
            /* Partial write — truncate is the simplest recovery */
            lfs_file_truncate(lfs, &f, (lfs_off_t)offset);
            lfs_file_close(lfs, &f);
            if (g_spi_mutex != NULL) { xSemaphoreGive(g_spi_mutex); }
            send_nak(cmd->seq, FLASH_MGR_ERR_NOSPC);
            return;
        }
    } else {
        /* Zero-length write → truncate at offset */
        lfs_file_truncate(lfs, &f, (lfs_off_t)offset);
    }

    lfs_file_close(lfs, &f);

    if (g_spi_mutex != NULL) { xSemaphoreGive(g_spi_mutex); }

    send_ack(cmd->seq);
}

/* ---- DELETE ----------------------------------------------------------
 *
 *   Request:  [path_len:1B][path:N]
 *   Response: ACK  or  NAK([err_code:1B])
 */

static void handle_delete(const Flash_mgr_cmd* cmd) {
    lfs_t* lfs = Lfs_Port_Get_Lfs(g_lfs_port);
    if (lfs == NULL) { send_nak(cmd->seq, FLASH_MGR_ERR_IO); return; }

    if (cmd->data_len < 1) { send_nak(cmd->seq, FLASH_MGR_ERR_INVAL); return; }

    uint8_t path_len = cmd->data[0];
    if (path_len == 0 || path_len > FLASH_MGR_PATH_MAX ||
        (uint32_t)(1 + path_len) > cmd->data_len) {
        send_nak(cmd->seq, FLASH_MGR_ERR_INVAL);
        return;
    }

    char path[FLASH_MGR_PATH_MAX + 1];
    memcpy(path, cmd->data + 1, path_len);
    path[path_len] = '\0';

    if (g_spi_mutex != NULL) { xSemaphoreTake(g_spi_mutex, portMAX_DELAY); }
    int err = lfs_remove(lfs, path);
    if (g_spi_mutex != NULL) { xSemaphoreGive(g_spi_mutex); }

    if (err < 0) {
        send_nak(cmd->seq, map_lfs_error(err));
    } else {
        send_ack(cmd->seq);
    }
}

/* ---- LIST ------------------------------------------------------------
 *
 *   Request:  [path_len:1B][path:N]
 *   Response: LIST_ITEM([type:1B][name_len:1B][name:N][size:4B LE]) × N
 *             LIST_END([count:2B LE])
 *
 *   Multiple response frames are sent for a single LIST request.
 *   "." and ".." are filtered out.
 */

static void handle_list(const Flash_mgr_cmd* cmd) {
    lfs_t* lfs = Lfs_Port_Get_Lfs(g_lfs_port);
    if (lfs == NULL) { send_nak(cmd->seq, FLASH_MGR_ERR_IO); return; }

    if (cmd->data_len < 1) { send_nak(cmd->seq, FLASH_MGR_ERR_INVAL); return; }

    uint8_t path_len = cmd->data[0];
    if (path_len > FLASH_MGR_PATH_MAX ||
        (uint32_t)(1 + path_len) > cmd->data_len) {
        send_nak(cmd->seq, FLASH_MGR_ERR_INVAL);
        return;
    }

    char path[FLASH_MGR_PATH_MAX + 1];
    if (path_len > 0) {
        memcpy(path, cmd->data + 1, path_len);
    }
    path[path_len] = '\0';

    if (g_spi_mutex != NULL) { xSemaphoreTake(g_spi_mutex, portMAX_DELAY); }

    lfs_dir_t dir;
    int err = lfs_dir_open(lfs, &dir, path_len > 0 ? path : "/");
    if (err < 0) {
        if (g_spi_mutex != NULL) { xSemaphoreGive(g_spi_mutex); }
        send_nak(cmd->seq, map_lfs_error(err));
        return;
    }

    uint16_t count = 0;
    struct lfs_info info;
    /* Static buffer for LIST_ITEM payload */
    static uint8_t list_buf[1 + 1 + LFS_NAME_MAX + 4];  /* type + name_len + name + size */

    while (lfs_dir_read(lfs, &dir, &info) > 0) {
        /* Skip "." and ".." */
        if (info.name[0] == '.' &&
            (info.name[1] == '\0' ||
             (info.name[1] == '.' && info.name[2] == '\0'))) {
            continue;
        }

        uint8_t name_len = (uint8_t)strlen(info.name);
        if (name_len > LFS_NAME_MAX) { name_len = LFS_NAME_MAX; }

        list_buf[0] = lfs_type_to_mgr_type(info.type);
        list_buf[1] = name_len;
        memcpy(list_buf + 2, info.name, name_len);
        list_buf[2 + name_len]     = (uint8_t)(info.size);
        list_buf[3 + name_len] = (uint8_t)(info.size >> 8);
        list_buf[4 + name_len] = (uint8_t)(info.size >> 16);
        list_buf[5 + name_len] = (uint8_t)(info.size >> 24);

        send_response(FLASH_MGR_RESP_LIST_ITEM, cmd->seq,
                      list_buf, (uint16_t)(2 + name_len + 4));
        count++;
    }

    lfs_dir_close(lfs, &dir);

    if (g_spi_mutex != NULL) { xSemaphoreGive(g_spi_mutex); }

    /* LIST_END: [count:2B LE] */
    uint8_t end_buf[2];
    end_buf[0] = (uint8_t)(count);
    end_buf[1] = (uint8_t)(count >> 8);
    send_response(FLASH_MGR_RESP_LIST_END, cmd->seq, end_buf, 2);
}

/* ---- INFO ------------------------------------------------------------
 *
 *   Request:  [path_len:1B][path:N]
 *   Response: INFO_RESP([type:1B][size:4B LE])  or  NAK([err_code:1B])
 */

static void handle_info(const Flash_mgr_cmd* cmd) {
    lfs_t* lfs = Lfs_Port_Get_Lfs(g_lfs_port);
    if (lfs == NULL) { send_nak(cmd->seq, FLASH_MGR_ERR_IO); return; }

    if (cmd->data_len < 1) { send_nak(cmd->seq, FLASH_MGR_ERR_INVAL); return; }

    uint8_t path_len = cmd->data[0];
    if (path_len == 0 || path_len > FLASH_MGR_PATH_MAX ||
        (uint32_t)(1 + path_len) > cmd->data_len) {
        send_nak(cmd->seq, FLASH_MGR_ERR_INVAL);
        return;
    }

    char path[FLASH_MGR_PATH_MAX + 1];
    memcpy(path, cmd->data + 1, path_len);
    path[path_len] = '\0';

    if (g_spi_mutex != NULL) { xSemaphoreTake(g_spi_mutex, portMAX_DELAY); }

    struct lfs_info info;
    int err = lfs_stat(lfs, path, &info);

    if (g_spi_mutex != NULL) { xSemaphoreGive(g_spi_mutex); }

    if (err < 0) {
        send_nak(cmd->seq, map_lfs_error(err));
        return;
    }

    /* INFO_RESP: [type:1B][size:4B LE] */
    uint8_t resp[5];
    resp[0] = lfs_type_to_mgr_type(info.type);
    resp[1] = (uint8_t)(info.size);
    resp[2] = (uint8_t)(info.size >> 8);
    resp[3] = (uint8_t)(info.size >> 16);
    resp[4] = (uint8_t)(info.size >> 24);

    send_response(FLASH_MGR_RESP_INFO_RESP, cmd->seq, resp, 5);
}

/* ---- FORMAT ----------------------------------------------------------
 *
 *   Request:  (no data)
 *   Response: ACK  or  NAK([err_code:1B])
 */

static void handle_format(const Flash_mgr_cmd* cmd) {
    if (g_lfs_port == NULL) { send_nak(cmd->seq, FLASH_MGR_ERR_IO); return; }

    if (g_spi_mutex != NULL) { xSemaphoreTake(g_spi_mutex, portMAX_DELAY); }

    /* Unmount, format, remount */
    int err = Lfs_Port_Unmount(g_lfs_port);
    if (err >= 0) {
        err = Lfs_Port_Format(g_lfs_port);
    }
    if (err >= 0) {
        err = Lfs_Port_Mount(g_lfs_port);
    }

    if (g_spi_mutex != NULL) { xSemaphoreGive(g_spi_mutex); }

    if (err < 0) {
        send_nak(cmd->seq, map_lfs_error(err));
    } else {
        send_ack(cmd->seq);
    }
}

/* ------------------------------------------------------------------ */
/* Initialisation                                                        */
/* ------------------------------------------------------------------ */

void flash_mgr_protocol_init(void) {
    /* ---- 1. Create SPI mutex ---- */
    g_spi_mutex = xSemaphoreCreateMutex();
    configASSERT(g_spi_mutex != NULL);

    /* ---- 2. Init W25Q32 ---- */
    const W25q32_config flash_cfg = {
        .spi_idx     = SPI_LCD_IDX,
        .cs_gpio_idx = GPIO_SPI_CS_IDX,
    };
    g_flash = W25q32_Create(&flash_cfg);
    configASSERT(g_flash != NULL);
    configASSERT(W25q32_Init(g_flash));

    /* ---- 3. Create lfs_port ---- */
    const Lfs_port_config port_cfg = {
        .flash     = g_flash,
        .start     = FLASH_MGR_LFS_START,
        .size      = FLASH_MGR_LFS_SIZE,
        .spi_mutex = g_spi_mutex,
    };
    g_lfs_port = Lfs_Port_Create(&port_cfg);
    configASSERT(g_lfs_port != NULL);

    /* ---- 4. Mount (attempt; if superblock absent, format then mount) ---- */
    int err = Lfs_Port_Mount(g_lfs_port);
    if (err != 0) {
        /* First boot or corrupted — format and mount */
        err = Lfs_Port_Format(g_lfs_port);
        if (err == 0) {
            err = Lfs_Port_Mount(g_lfs_port);
        }
        configASSERT(err == 0);
    }

    /* ---- 5. Create command queue ---- */
    g_cmd_queue = xQueueCreate(FLASH_MGR_QUEUE_DEPTH, sizeof(Flash_mgr_cmd));
    configASSERT(g_cmd_queue != NULL);

    /* ---- 6. Create com_uart with binary framing protocol ---- */
    static const Com_uart_config uart_cfg = {
        .uart_idx             = FLASH_MGR_UART_IDX,
        .idle_timeout_ms      = FLASH_MGR_IDLE_TIMEOUT_MS,
        .rx_max_len           = FLASH_MGR_RX_MAX_LEN,
        .tx_max_len           = FLASH_MGR_TX_MAX_LEN,
        .protocol_type        = protocol_binary_frame,
        .protocol_max_payload = FLASH_MGR_PROTO_MAX_PAYLOAD,
        .on_rx                = flash_mgr_on_rx,
        .on_rx_arg            = NULL,
    };
    g_com_uart = Com_Uart_Create(&uart_cfg);
    configASSERT(g_com_uart != NULL);

    /* ---- 7. Create flash_mgr task ---- */
    BaseType_t ret = xTaskCreate(
        flash_mgr_task,
        "FlashMgr",
        FLASH_MGR_TASK_STACK_WORDS,
        NULL,
        FLASH_MGR_TASK_PRIORITY,
        &g_task_handle);
    configASSERT(ret == pdPASS);
}

// clang-format on

#else

    #include <stddef.h>

void flash_mgr_protocol_init(void) { (void)0; }

#endif
