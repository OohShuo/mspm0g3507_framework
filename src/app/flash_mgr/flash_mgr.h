#pragma once

#if FRAMEWORK_USE_LFS

// clang-format off

#include <stdint.h>

#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"

/* ------------------------------------------------------------------ */
/* Protocol constants                                                   */
/* ------------------------------------------------------------------ */

#define FLASH_MGR_SYNC0         0xAAu
#define FLASH_MGR_SYNC1         0x55u
#define FLASH_MGR_CHUNK_SIZE    512u
#define FLASH_MGR_PATH_MAX      255u
#define FLASH_MGR_TX_BUF_SIZE   530u  /* 2(sync)+1(cmd)+2(seq)+2(len)+512(data)+2(crc)+9 */

/* ---- host → device commands --------------------------------------- */

#define FLASH_MGR_CMD_READ      0x01u
#define FLASH_MGR_CMD_WRITE     0x02u
#define FLASH_MGR_CMD_DELETE    0x03u
#define FLASH_MGR_CMD_LIST      0x04u
#define FLASH_MGR_CMD_INFO      0x05u
#define FLASH_MGR_CMD_FORMAT    0x06u
#define FLASH_MGR_CMD_RESET     0x07u

/* ---- device → host responses -------------------------------------- */

#define FLASH_MGR_RESP_ACK       0x80u
#define FLASH_MGR_RESP_NAK       0x81u
#define FLASH_MGR_RESP_CRC_ERR   0x82u
#define FLASH_MGR_RESP_BUSY      0x83u
#define FLASH_MGR_RESP_CHUNK     0x84u
#define FLASH_MGR_RESP_EOF       0x85u
#define FLASH_MGR_RESP_LIST_ITEM 0x86u
#define FLASH_MGR_RESP_LIST_END  0x87u
#define FLASH_MGR_RESP_INFO_RESP 0x88u

/* ---- NAK error codes ---------------------------------------------- */

#define FLASH_MGR_ERR_UNKNOWN    0x00u
#define FLASH_MGR_ERR_NOENT      0x01u
#define FLASH_MGR_ERR_NOSPC      0x02u
#define FLASH_MGR_ERR_INVAL      0x03u
#define FLASH_MGR_ERR_EXIST      0x04u
#define FLASH_MGR_ERR_IO         0x05u
#define FLASH_MGR_ERR_CORRUPT    0x06u

/* ---- file type codes (LIST_ITEM / INFO_RESP) ---------------------- */

#define FLASH_MGR_TYPE_UNKNOWN   0x00u
#define FLASH_MGR_TYPE_REG       0x01u
#define FLASH_MGR_TYPE_DIR       0x02u

/* ------------------------------------------------------------------ */
/* Queue command descriptor                                             */
/* ------------------------------------------------------------------ */

typedef struct {
    uint8_t  cmd;
    uint16_t seq;
    uint16_t data_len;
    uint8_t  data[FLASH_MGR_CHUNK_SIZE];
} Flash_mgr_cmd;

/* ------------------------------------------------------------------ */
/* Public API                                                           */
/* ------------------------------------------------------------------ */

/**
 * @brief One-time initialisation of the Flash Manager subsystem.
 *
 * Creates the W25Q32 handle, lfs_port, FreeRTOS queue, mutex, com_uart
 * instance, and the flash_mgr task.  Must be called after Bsp_Init() /
 * Hal_Init() / Com_Uart_Init().
 */
void Flash_Mgr_Init(void);

// clang-format on

#endif
