// clang-format off

#include "flash_mgr.h"

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#if FRAMEWORK_USE_LFS

#include <stddef.h>
#include <string.h>

#include "bsp_uart.h"
#include "com_uart.h"
#include "lfs.h"
#include "rtt_log.h"
#include "storage.h"

#define FLASH_MGR_UART_IDX          0u
#define FLASH_MGR_RX_MAX_LEN        600u
#define FLASH_MGR_TX_MAX_LEN        600u
#define FLASH_MGR_IDLE_TIMEOUT_MS   5u
#define FLASH_MGR_PROTO_MAX_PAYLOAD 520u /* CMD(1)+SEQ(2)+data(512)+offset(4) max = 519 */

// clang-format on

static Com_uart* g_com_uart = NULL;
static QueueHandle_t g_cmd_queue = NULL;

static uint8_t g_tx_buf[FLASH_MGR_TX_BUF_SIZE];

static void flash_mgr_on_rx(Com_uart* obj, const uint8_t* data, uint32_t len, uint8_t flags, void* arg);
static void send_response(uint8_t cmd, uint16_t seq, const uint8_t* data, uint16_t data_len);
static void send_ack(uint16_t seq);
static void send_nak(uint16_t seq, uint8_t err_code);

static void handle_read(const Flash_mgr_cmd* cmd);
static void handle_write(const Flash_mgr_cmd* cmd);
static void handle_delete(const Flash_mgr_cmd* cmd);
static void handle_list(const Flash_mgr_cmd* cmd);
static void handle_info(const Flash_mgr_cmd* cmd);
static void handle_format(const Flash_mgr_cmd* cmd);

static uint8_t map_lfs_error(int lfs_err) {
    switch (lfs_err) {
        case 0:
            return 0;
        case LFS_ERR_NOENT:
            return FLASH_MGR_ERR_NOENT;
        case LFS_ERR_NOSPC:
            return FLASH_MGR_ERR_NOSPC;
        case LFS_ERR_IO:
            return FLASH_MGR_ERR_IO;
        case LFS_ERR_CORRUPT:
            return FLASH_MGR_ERR_CORRUPT;
        case LFS_ERR_EXIST:
            return FLASH_MGR_ERR_EXIST;
        case LFS_ERR_INVAL:
            return FLASH_MGR_ERR_INVAL;
        default:
            return FLASH_MGR_ERR_UNKNOWN;
    }
}

static uint8_t lfs_type_to_mgr_type(uint8_t lfs_type) {
    if (lfs_type == LFS_TYPE_REG) { return FLASH_MGR_TYPE_REG; }
    if (lfs_type == LFS_TYPE_DIR) { return FLASH_MGR_TYPE_DIR; }
    return FLASH_MGR_TYPE_UNKNOWN;
}

static void send_response(uint8_t cmd, uint16_t seq, const uint8_t* data, uint16_t data_len) {
    if (g_com_uart == NULL) { return; }
    Bsp_Uart_Wait_For_Complete(g_com_uart->config.uart_idx);

    g_tx_buf[0] = cmd;
    g_tx_buf[1] = (uint8_t)(seq >> 8);
    g_tx_buf[2] = (uint8_t)(seq);
    if (data_len > 0 && data != NULL) { memcpy(g_tx_buf + 3, data, data_len); }
    Com_Uart_Send(g_com_uart, g_tx_buf, (uint32_t)(3u + data_len));
}

static void send_ack(uint16_t seq) { send_response(FLASH_MGR_RESP_ACK, seq, NULL, 0); }

static void send_nak(uint16_t seq, uint8_t err_code) { send_response(FLASH_MGR_RESP_NAK, seq, &err_code, 1); }

static void flash_mgr_on_rx(Com_uart* obj, const uint8_t* data, uint32_t len, uint8_t flags, void* arg) {
    (void)obj;
    (void)arg;
    (void)flags;

    if (len < 3) { return; }

    uint8_t cmd = data[0];
    uint16_t seq = ((uint16_t)data[1] << 8) | data[2];
    uint16_t payload_len = (uint16_t)(len - 3u);

    if (cmd < FLASH_MGR_CMD_READ || cmd > FLASH_MGR_CMD_RESET) {
        send_nak(seq, FLASH_MGR_ERR_INVAL);
        return;
    }

    Flash_mgr_cmd queue_item;
    queue_item.cmd = cmd;
    queue_item.seq = seq;

    if (payload_len > sizeof(queue_item.data)) { payload_len = (uint16_t)sizeof(queue_item.data); }
    queue_item.data_len = payload_len;
    if (payload_len > 0) { memcpy(queue_item.data, data + 3, payload_len); }

    if (g_cmd_queue != NULL) {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;  // NOLINT (readability-identifier-naming)
        BaseType_t ok = xQueueSendToBackFromISR(g_cmd_queue, &queue_item, &xHigherPriorityTaskWoken);
        if (ok != pdPASS) { send_response(FLASH_MGR_RESP_BUSY, seq, NULL, 0); }
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

static void flash_mgr_loop(void* arg) {
    (void)arg;

    Flash_mgr_cmd cmd;

    while (1) {
        if (xQueueReceive(g_cmd_queue, &cmd, portMAX_DELAY) != pdTRUE) { continue; }

        switch (cmd.cmd) {
            case FLASH_MGR_CMD_READ:
                handle_read(&cmd);
                break;
            case FLASH_MGR_CMD_WRITE:
                handle_write(&cmd);
                break;
            case FLASH_MGR_CMD_DELETE:
                handle_delete(&cmd);
                break;
            case FLASH_MGR_CMD_LIST:
                handle_list(&cmd);
                break;
            case FLASH_MGR_CMD_INFO:
                handle_info(&cmd);
                break;
            case FLASH_MGR_CMD_FORMAT:
                handle_format(&cmd);
                break;
            case FLASH_MGR_CMD_RESET:
                send_ack(cmd.seq);
                break;
            default:
                send_nak(cmd.seq, FLASH_MGR_ERR_INVAL);
                break;
        }
    }
}

static void handle_read(const Flash_mgr_cmd* cmd) {
    lfs_t* lfs = Storage_Get_Lfs();
    if (lfs == NULL) {
        send_nak(cmd->seq, FLASH_MGR_ERR_IO);
        return;
    }

    if (cmd->data_len < 5) {
        send_nak(cmd->seq, FLASH_MGR_ERR_INVAL);
        return;
    }

    uint8_t path_len = cmd->data[0];
    if (path_len == 0 || path_len > FLASH_MGR_PATH_MAX || (uint32_t)(1 + path_len + 4) > cmd->data_len) {
        send_nak(cmd->seq, FLASH_MGR_ERR_INVAL);
        return;
    }

    uint32_t offset = (uint32_t)cmd->data[1 + path_len] | ((uint32_t)cmd->data[2 + path_len] << 8) |
                      ((uint32_t)cmd->data[3 + path_len] << 16) | ((uint32_t)cmd->data[4 + path_len] << 24);

    char path[FLASH_MGR_PATH_MAX + 1];
    memcpy(path, cmd->data + 1, path_len);
    path[path_len] = '\0';

    Storage_Lock();

    lfs_file_t f;
    int err = lfs_file_open(lfs, &f, path, LFS_O_RDONLY);
    if (err < 0) {
        Storage_Unlock();
        send_nak(cmd->seq, map_lfs_error(err));
        return;
    }

    err = lfs_file_seek(lfs, &f, (lfs_soff_t)offset, LFS_SEEK_SET);
    if (err < 0) {
        lfs_file_close(lfs, &f);
        Storage_Unlock();
        send_nak(cmd->seq, map_lfs_error(err));
        return;
    }

    static uint8_t chunk_buf[FLASH_MGR_CHUNK_SIZE];
    lfs_ssize_t nread = lfs_file_read(lfs, &f, chunk_buf, FLASH_MGR_CHUNK_SIZE);

    lfs_soff_t file_size = lfs_file_size(lfs, &f);
    lfs_file_close(lfs, &f);

    Storage_Unlock();

    if (nread < 0) {
        send_nak(cmd->seq, map_lfs_error((int)nread));
        return;
    }

    if (nread == 0) {
        uint8_t eof_data[4];
        eof_data[0] = (uint8_t)(file_size);
        eof_data[1] = (uint8_t)(file_size >> 8);
        eof_data[2] = (uint8_t)(file_size >> 16);
        eof_data[3] = (uint8_t)(file_size >> 24);
        send_response(FLASH_MGR_RESP_EOF, cmd->seq, eof_data, 4);
    } else {
        static uint8_t chunk_resp[4 + FLASH_MGR_CHUNK_SIZE];
        chunk_resp[0] = (uint8_t)(offset);
        chunk_resp[1] = (uint8_t)(offset >> 8);
        chunk_resp[2] = (uint8_t)(offset >> 16);
        chunk_resp[3] = (uint8_t)(offset >> 24);
        memcpy(chunk_resp + 4, chunk_buf, (size_t)nread);

        send_response(FLASH_MGR_RESP_CHUNK, cmd->seq, chunk_resp, (uint16_t)(4 + nread));
    }
}

static void handle_write(const Flash_mgr_cmd* cmd) {
    lfs_t* lfs = Storage_Get_Lfs();
    if (lfs == NULL) {
        send_nak(cmd->seq, FLASH_MGR_ERR_IO);
        return;
    }

    if (cmd->data_len < 5) {
        send_nak(cmd->seq, FLASH_MGR_ERR_INVAL);
        return;
    }

    uint8_t path_len = cmd->data[0];

    if (path_len == 0 || path_len > FLASH_MGR_PATH_MAX || (uint32_t)(1 + path_len + 4) > cmd->data_len) {
        send_nak(cmd->seq, FLASH_MGR_ERR_INVAL);
        return;
    }

    uint32_t offset = (uint32_t)cmd->data[1 + path_len] | ((uint32_t)cmd->data[2 + path_len] << 8) |
                      ((uint32_t)cmd->data[3 + path_len] << 16) | ((uint32_t)cmd->data[4 + path_len] << 24);

    uint16_t header_size = (uint16_t)(1 + path_len + 4);
    uint16_t chunk_len = cmd->data_len - header_size;

    char path[FLASH_MGR_PATH_MAX + 1];
    memcpy(path, cmd->data + 1, path_len);
    path[path_len] = '\0';

    const uint8_t* chunk_data = cmd->data + header_size;

    Storage_Lock();

    lfs_file_t f;
    int err = lfs_file_open(lfs, &f, path, LFS_O_WRONLY | LFS_O_CREAT);
    if (err < 0) {
        Storage_Unlock();
        send_nak(cmd->seq, map_lfs_error(err));
        return;
    }

    err = lfs_file_seek(lfs, &f, (lfs_soff_t)offset, LFS_SEEK_SET);
    if (err < 0) {
        lfs_file_close(lfs, &f);
        Storage_Unlock();
        send_nak(cmd->seq, map_lfs_error(err));
        return;
    }

    if (chunk_len > 0) {
        lfs_ssize_t written = lfs_file_write(lfs, &f, chunk_data, chunk_len);
        if (written < 0) {
            lfs_file_close(lfs, &f);
            Storage_Unlock();
            send_nak(cmd->seq, map_lfs_error((int)written));
            return;
        }
        if ((uint16_t)written != chunk_len) {
            /* Partial write — truncate is the simplest recovery */
            lfs_file_truncate(lfs, &f, (lfs_off_t)offset);
            lfs_file_close(lfs, &f);
            Storage_Unlock();
            send_nak(cmd->seq, FLASH_MGR_ERR_NOSPC);
            return;
        }
    } else {
        lfs_file_truncate(lfs, &f, (lfs_off_t)offset);
    }

    lfs_file_close(lfs, &f);

    Storage_Unlock();

    send_ack(cmd->seq);
}

static void handle_delete(const Flash_mgr_cmd* cmd) {
    lfs_t* lfs = Storage_Get_Lfs();
    if (lfs == NULL) {
        send_nak(cmd->seq, FLASH_MGR_ERR_IO);
        return;
    }

    if (cmd->data_len < 1) {
        send_nak(cmd->seq, FLASH_MGR_ERR_INVAL);
        return;
    }

    uint8_t path_len = cmd->data[0];
    if (path_len == 0 || path_len > FLASH_MGR_PATH_MAX || (uint32_t)(1 + path_len) > cmd->data_len) {
        send_nak(cmd->seq, FLASH_MGR_ERR_INVAL);
        return;
    }

    char path[FLASH_MGR_PATH_MAX + 1];
    memcpy(path, cmd->data + 1, path_len);
    path[path_len] = '\0';

    Storage_Lock();
    int err = lfs_remove(lfs, path);
    Storage_Unlock();

    if (err < 0) {
        send_nak(cmd->seq, map_lfs_error(err));
    } else {
        send_ack(cmd->seq);
    }
}

static void handle_list(const Flash_mgr_cmd* cmd) {
    lfs_t* lfs = Storage_Get_Lfs();
    if (lfs == NULL) {
        send_nak(cmd->seq, FLASH_MGR_ERR_IO);
        return;
    }

    if (cmd->data_len < 1) {
        send_nak(cmd->seq, FLASH_MGR_ERR_INVAL);
        return;
    }

    uint8_t path_len = cmd->data[0];
    if (path_len > FLASH_MGR_PATH_MAX || (uint32_t)(1 + path_len) > cmd->data_len) {
        send_nak(cmd->seq, FLASH_MGR_ERR_INVAL);
        return;
    }

    char path[FLASH_MGR_PATH_MAX + 1];
    if (path_len > 0) { memcpy(path, cmd->data + 1, path_len); }
    path[path_len] = '\0';

    Storage_Lock();

    lfs_dir_t dir;
    int err = lfs_dir_open(lfs, &dir, path_len > 0 ? path : "/");
    if (err < 0) {
        Storage_Unlock();
        send_nak(cmd->seq, map_lfs_error(err));
        return;
    }

    uint16_t count = 0;
    struct lfs_info info;
    static uint8_t list_buf[1 + 1 + LFS_NAME_MAX + 4];

    while (lfs_dir_read(lfs, &dir, &info) > 0) {
        if (info.name[0] == '.' && (info.name[1] == '\0' || (info.name[1] == '.' && info.name[2] == '\0'))) {
            continue;
        }

        uint8_t name_len = (uint8_t)strlen(info.name);
        if (name_len > LFS_NAME_MAX) { name_len = LFS_NAME_MAX; }

        list_buf[0] = lfs_type_to_mgr_type(info.type);
        list_buf[1] = name_len;
        memcpy(list_buf + 2, info.name, name_len);
        list_buf[2 + name_len] = (uint8_t)(info.size);
        list_buf[3 + name_len] = (uint8_t)(info.size >> 8);
        list_buf[4 + name_len] = (uint8_t)(info.size >> 16);
        list_buf[5 + name_len] = (uint8_t)(info.size >> 24);

        send_response(FLASH_MGR_RESP_LIST_ITEM, cmd->seq, list_buf, (uint16_t)(2 + name_len + 4));
        count++;
    }

    lfs_dir_close(lfs, &dir);

    Storage_Unlock();

    uint8_t end_buf[2];
    end_buf[0] = (uint8_t)(count);
    end_buf[1] = (uint8_t)(count >> 8);
    send_response(FLASH_MGR_RESP_LIST_END, cmd->seq, end_buf, 2);
}

static void handle_info(const Flash_mgr_cmd* cmd) {
    lfs_t* lfs = Storage_Get_Lfs();
    if (lfs == NULL) {
        send_nak(cmd->seq, FLASH_MGR_ERR_IO);
        return;
    }

    if (cmd->data_len < 1) {
        send_nak(cmd->seq, FLASH_MGR_ERR_INVAL);
        return;
    }

    uint8_t path_len = cmd->data[0];
    if (path_len == 0 || path_len > FLASH_MGR_PATH_MAX || (uint32_t)(1 + path_len) > cmd->data_len) {
        send_nak(cmd->seq, FLASH_MGR_ERR_INVAL);
        return;
    }

    char path[FLASH_MGR_PATH_MAX + 1];
    memcpy(path, cmd->data + 1, path_len);
    path[path_len] = '\0';

    Storage_Lock();

    struct lfs_info info;
    int err = lfs_stat(lfs, path, &info);

    Storage_Unlock();

    if (err < 0) {
        send_nak(cmd->seq, map_lfs_error(err));
        return;
    }

    uint8_t resp[5];
    resp[0] = lfs_type_to_mgr_type(info.type);
    resp[1] = (uint8_t)(info.size);
    resp[2] = (uint8_t)(info.size >> 8);
    resp[3] = (uint8_t)(info.size >> 16);
    resp[4] = (uint8_t)(info.size >> 24);

    send_response(FLASH_MGR_RESP_INFO_RESP, cmd->seq, resp, 5);
}

static void handle_format(const Flash_mgr_cmd* cmd) {
    if (!Storage_Is_Available()) {
        send_nak(cmd->seq, FLASH_MGR_ERR_IO);
        return;
    }

    if (!Storage_Format()) {
        send_nak(cmd->seq, FLASH_MGR_ERR_IO);
    } else {
        send_ack(cmd->seq);
    }
}

static void flash_mgr_init(void) {
    configASSERT(Storage_Is_Available());

    g_cmd_queue = xQueueCreate(FLASH_MGR_QUEUE_DEPTH, sizeof(Flash_mgr_cmd));
    configASSERT(g_cmd_queue != NULL);

    static const Com_uart_config uart_cfg = {
        .uart_idx = FLASH_MGR_UART_IDX,
        .idle_timeout_ms = FLASH_MGR_IDLE_TIMEOUT_MS,
        .rx_max_len = FLASH_MGR_RX_MAX_LEN,
        .tx_max_len = FLASH_MGR_TX_MAX_LEN,
        .protocol_type = protocol_binary_frame,
        .protocol_max_payload = FLASH_MGR_PROTO_MAX_PAYLOAD,
        .on_rx = flash_mgr_on_rx,
        .on_rx_arg = NULL,
    };
    g_com_uart = Com_Uart_Create(&uart_cfg);
    configASSERT(g_com_uart != NULL);
}

static void flash_mgr_task(void* arg) {
    flash_mgr_init();
    uint32_t tick = xTaskGetTickCount();
    while (1) {
        flash_mgr_loop(arg);

        vTaskDelayUntil(&tick, pdMS_TO_TICKS(10));
    }
}

#else /* !FRAMEWORK_USE_LFS */

static void flash_mgr_task(void* arg) {
    uint32_t tick = xTaskGetTickCount();
    while (1) { vTaskDelayUntil(&tick, pdMS_TO_TICKS(5000)); }
}

#endif /* FRAMEWORK_USE_LFS */

void Flash_Mgr_Task_Def(void) {
    BaseType_t ret = xTaskCreate(flash_mgr_task, "FlashMgr", 1024, NULL, 1, NULL);
    configASSERT(ret == pdPASS);
}
