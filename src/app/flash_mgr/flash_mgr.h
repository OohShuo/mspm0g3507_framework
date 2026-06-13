#pragma once

#include <stdint.h>

typedef struct {
    uint8_t cmd;
    uint16_t seq;
    uint16_t data_len;
    uint8_t data[517]; /* max payload (520) - CMD(1) - SEQ(2) */
} Flash_mgr_cmd;

// Protocol constants

#define FLASH_MGR_SYNC0          0xAAu
#define FLASH_MGR_SYNC1          0x55u
#define FLASH_MGR_CHUNK_SIZE     512u
#define FLASH_MGR_PATH_MAX       255u
#define FLASH_MGR_TX_BUF_SIZE    530u /* 2+1+2+2+512+2+9 headroom */

// host → device commands

#define FLASH_MGR_CMD_READ       0x01u
#define FLASH_MGR_CMD_WRITE      0x02u
#define FLASH_MGR_CMD_DELETE     0x03u
#define FLASH_MGR_CMD_LIST       0x04u
#define FLASH_MGR_CMD_INFO       0x05u
#define FLASH_MGR_CMD_FORMAT     0x06u
#define FLASH_MGR_CMD_RESET      0x07u

// device → host responses

#define FLASH_MGR_RESP_ACK       0x80u
#define FLASH_MGR_RESP_NAK       0x81u
#define FLASH_MGR_RESP_CRC_ERR   0x82u
#define FLASH_MGR_RESP_BUSY      0x83u
#define FLASH_MGR_RESP_CHUNK     0x84u
#define FLASH_MGR_RESP_EOF       0x85u
#define FLASH_MGR_RESP_LIST_ITEM 0x86u
#define FLASH_MGR_RESP_LIST_END  0x87u
#define FLASH_MGR_RESP_INFO_RESP 0x88u

// NAK error codes

#define FLASH_MGR_ERR_UNKNOWN    0x00u
#define FLASH_MGR_ERR_NOENT      0x01u
#define FLASH_MGR_ERR_NOSPC      0x02u
#define FLASH_MGR_ERR_INVAL      0x03u
#define FLASH_MGR_ERR_EXIST      0x04u
#define FLASH_MGR_ERR_IO         0x05u
#define FLASH_MGR_ERR_CORRUPT    0x06u

// file type codes

#define FLASH_MGR_TYPE_UNKNOWN   0x00u
#define FLASH_MGR_TYPE_REG       0x01u
#define FLASH_MGR_TYPE_DIR       0x02u

// queue config

#define FLASH_MGR_QUEUE_DEPTH    4u

void Flash_Mgr_Task_Def(void);
