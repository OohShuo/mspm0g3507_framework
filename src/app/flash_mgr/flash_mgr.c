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
#include "soft_crc.h"
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
/* UART config                                                          */
/*                                                                     */
/* Reuses UART_DEBUG_IDX (UART0) that com_uart_test and slip_recv use. */
/* RX buffer: 521 B max frame → 600 B to leave headroom.               */
/* TX buffer: same size so the task can send a full response.           */
/* Idle timeout: 5 ms — short enough to feel responsive, long enough    */
/* that a 521-byte frame at 115200 bps (~45 ms) won't be split inside. */
/* ------------------------------------------------------------------ */

#define FLASH_MGR_UART_IDX         0u
#define FLASH_MGR_RX_MAX_LEN       600u
#define FLASH_MGR_TX_MAX_LEN       600u
#define FLASH_MGR_IDLE_TIMEOUT_MS  5u

/* ------------------------------------------------------------------ */
/* Task / queue config                                                  */
/* ------------------------------------------------------------------ */

#define FLASH_MGR_TASK_STACK_WORDS 768u
#define FLASH_MGR_TASK_PRIORITY    2u
#define FLASH_MGR_QUEUE_DEPTH      4u

/* ------------------------------------------------------------------ */
/* Frame-parser states                                                  */
/* ------------------------------------------------------------------ */

enum {
    FRAME_STATE_SYNC0,
    FRAME_STATE_SYNC1,
    FRAME_STATE_HEADER,
    FRAME_STATE_DATA,
    FRAME_STATE_CRC,
};

/* ------------------------------------------------------------------ */
/* Static module state                                                  */
/* ------------------------------------------------------------------ */

static Com_uart*     g_com_uart     = NULL;
static W25q32*       g_flash        = NULL;
static Lfs_port*     g_lfs_port     = NULL;
static SemaphoreHandle_t g_spi_mutex = NULL;
static QueueHandle_t g_cmd_queue    = NULL;
static TaskHandle_t  g_task_handle  = NULL;

/* ---- Frame parser state (only accessed from UART rx callback) ---- */

static uint8_t  g_rx_state      = FRAME_STATE_SYNC0;
static uint8_t  g_rx_header[5];           /* CMD(1) + SEQ(2) + LEN(2)  */
static uint8_t  g_rx_header_pos = 0;
static uint8_t  g_rx_data[FLASH_MGR_CHUNK_SIZE];
static uint16_t g_rx_data_len   = 0;
static uint16_t g_rx_data_pos   = 0;
static uint8_t  g_rx_crc_buf[2];
static uint8_t  g_rx_crc_pos    = 0;

/* ---- Last-processed SEQ for duplicate detection ------------------- */

static uint16_t g_last_seq     = 0;
static uint8_t  g_has_last_seq = 0;

/* ---- Response buffer (used by flash_mgr task) --------------------- */

static uint8_t g_tx_buf[FLASH_MGR_TX_BUF_SIZE];

/* ------------------------------------------------------------------ */
/* Forward declarations                                                 */
/* ------------------------------------------------------------------ */

static void flash_mgr_task(void* arg);
static void flash_mgr_on_rx(Com_uart* obj, const uint8_t* data, uint32_t len, void* arg);
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
/* Frame building                                                       */
/*                                                                     */
/* Layout: SYNC0 SYNC1 CMD SEQ_H SEQ_L LEN_H LEN_L [DATA...] CRCH CRCL */
/* CRC16 covers CMD + SEQ(2) + LEN(2) + DATA.                          */
/* ------------------------------------------------------------------ */

static uint32_t build_frame(uint8_t* buf, uint8_t cmd, uint16_t seq,
                             const uint8_t* data, uint16_t data_len) {
    buf[0] = FLASH_MGR_SYNC0;
    buf[1] = FLASH_MGR_SYNC1;
    buf[2] = cmd;
    buf[3] = (uint8_t)(seq >> 8);
    buf[4] = (uint8_t)(seq);
    buf[5] = (uint8_t)(data_len >> 8);
    buf[6] = (uint8_t)(data_len);

    if (data_len > 0 && data != NULL) {
        memcpy(buf + 7, data, data_len);
    }

    /* CRC over CMD(1) + SEQ(2) + LEN(2) + DATA(data_len) = 5 + data_len */
    uint16_t crc = Soft_Crc16_Calc(soft_crc16_default, buf + 2, (uint32_t)(5 + data_len));
    buf[7 + data_len]     = (uint8_t)(crc >> 8);
    buf[8 + data_len] = (uint8_t)(crc);

    return 9u + data_len;
}

/* ------------------------------------------------------------------ */
/* Response helpers (called from flash_mgr task)                        */
/* ------------------------------------------------------------------ */

static void send_response(uint8_t cmd, uint16_t seq,
                          const uint8_t* data, uint16_t data_len) {
    if (g_com_uart == NULL) { return; }
    /* Wait for previous DMA transfer to finish so we don't overwrite
     * the TX buffer while the UART is still reading from it.  Essential
     * for LIST where multiple frames are sent back-to-back. */
    Bsp_Uart_Wait_For_Complete(g_com_uart->config.uart_idx);
    uint32_t frame_len = build_frame(g_tx_buf, cmd, seq, data, data_len);
    Com_Uart_Send(g_com_uart, g_tx_buf, frame_len);
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

static void flash_mgr_on_rx(Com_uart* obj, const uint8_t* data, uint32_t len, void* arg) {
    (void)obj;
    (void)arg;

    for (uint32_t i = 0; i < len; i++) {
        uint8_t byte = data[i];

        switch (g_rx_state) {

        case FRAME_STATE_SYNC0:
            if (byte == FLASH_MGR_SYNC0) {
                g_rx_state = FRAME_STATE_SYNC1;
            }
            break;

        case FRAME_STATE_SYNC1:
            if (byte == FLASH_MGR_SYNC1) {
                g_rx_state       = FRAME_STATE_HEADER;
                g_rx_header_pos  = 0;
            } else if (byte != FLASH_MGR_SYNC0) {
                g_rx_state = FRAME_STATE_SYNC0;
            }
            /* If byte == SYNC0, stay in SYNC1 (0xAA 0xAA 0x55 is valid) */
            break;

        case FRAME_STATE_HEADER:
            g_rx_header[g_rx_header_pos++] = byte;
            if (g_rx_header_pos >= 5) {
                /* Parse LEN (big-endian) */
                g_rx_data_len = ((uint16_t)g_rx_header[3] << 8) | g_rx_header[4];
                if (g_rx_data_len > FLASH_MGR_CHUNK_SIZE) {
                    /* Invalid length — reset */
                    g_rx_state = FRAME_STATE_SYNC0;
                } else if (g_rx_data_len == 0) {
                    /* No data payload — go straight to CRC */
                    g_rx_state    = FRAME_STATE_CRC;
                    g_rx_crc_pos  = 0;
                } else {
                    g_rx_state    = FRAME_STATE_DATA;
                    g_rx_data_pos = 0;
                }
            }
            break;

        case FRAME_STATE_DATA:
            g_rx_data[g_rx_data_pos++] = byte;
            if (g_rx_data_pos >= g_rx_data_len) {
                g_rx_state   = FRAME_STATE_CRC;
                g_rx_crc_pos = 0;
            }
            break;

        case FRAME_STATE_CRC:
            g_rx_crc_buf[g_rx_crc_pos++] = byte;
            if (g_rx_crc_pos >= 2) {
                /* ---- frame complete — verify CRC ---- */
                g_rx_state = FRAME_STATE_SYNC0;  /* ready for next frame */

                uint16_t seq    = ((uint16_t)g_rx_header[1] << 8) | g_rx_header[2];
                uint8_t  cmd    = g_rx_header[0];

                /* Build a temporary buffer for CRC calculation.
                 * It must contain: CMD(1) + SEQ(2) + LEN(2) + DATA(data_len).
                 * That's exactly the 6 header bytes + data. */
                uint16_t expected_crc = ((uint16_t)g_rx_crc_buf[0] << 8) | g_rx_crc_buf[1];

                /* CRC covers CMD(1)+SEQ(2)+LEN(2)+DATA(data_len).
                 * Soft_Crc16_Calc is stateless (resets each call), so we
                 * must feed header+data in a single contiguous call.
                 * g_tx_buf (530 B) is free during the RX path — reuse it. */
                uint16_t computed_crc;
                uint32_t crc_span = 5u + g_rx_data_len;
                memcpy(g_tx_buf, g_rx_header, 5);
                if (g_rx_data_len > 0) {
                    memcpy(g_tx_buf + 5, g_rx_data, g_rx_data_len);
                }
                computed_crc = Soft_Crc16_Calc(
                    soft_crc16_default, g_tx_buf, crc_span);

                if (computed_crc != expected_crc) {
                    /* CRC mismatch — request retransmission */
                    send_response(FLASH_MGR_RESP_CRC_ERR, seq, NULL, 0);
                    break;
                }

                /* ---- CRC OK — dispatch ---- */

                /* Duplicate detection: if SEQ matches last processed,
                 * re-send ACK and drop.  This handles the case where our
                 * response was lost and the host retransmitted. */
                if (g_has_last_seq && seq == g_last_seq) {
                    send_ack(seq);
                    break;
                }

                /* Validate command byte */
                if (cmd < FLASH_MGR_CMD_READ || cmd > FLASH_MGR_CMD_RESET) {
                    send_nak(seq, FLASH_MGR_ERR_INVAL);
                    break;
                }

                /* Copy to queue item and send to flash_mgr task */
                Flash_mgr_cmd queue_item;
                queue_item.cmd      = cmd;
                queue_item.seq      = seq;
                queue_item.data_len = g_rx_data_len;
                if (g_rx_data_len > 0) {
                    memcpy(queue_item.data, g_rx_data, g_rx_data_len);
                }

                if (g_cmd_queue != NULL) {
                    BaseType_t ok = xQueueSendToBack(g_cmd_queue, &queue_item, 0);
                    if (ok != pdPASS) {
                        /* Queue full — device busy */
                        send_response(FLASH_MGR_RESP_BUSY, seq, NULL, 0);
                    }
                }

                /* Update last-processed SEQ */
                g_last_seq     = seq;
                g_has_last_seq = 1;
            }
            break;
        } /* switch */
    }
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

void Flash_Mgr_Init(void) {
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

    /* ---- 6. Create com_uart for protocol ---- */
    static const Com_uart_config uart_cfg = {
        .uart_idx       = FLASH_MGR_UART_IDX,
        .idle_timeout_ms = FLASH_MGR_IDLE_TIMEOUT_MS,
        .rx_max_len     = FLASH_MGR_RX_MAX_LEN,
        .tx_max_len     = FLASH_MGR_TX_MAX_LEN,
        .on_rx          = flash_mgr_on_rx,
        .on_rx_arg      = NULL,
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

void Flash_Mgr_Init(void) { (void)0; }

#endif
