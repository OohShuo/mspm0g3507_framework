#include "general_data.h"

#include <string.h>

#include "soft_crc.h"

uint8_t General_Data_Check(const uint8_t* buffer, uint32_t buf_len) {
    if (buffer == NULL) { return 0; }
    if (buf_len < 3) { return 0; }
    if ((uint32_t)buffer[0] + 3u != buf_len) { return 0; }

    uint16_t crc_rx;
    memcpy(&crc_rx, buffer + buf_len - 2, 2);
    uint16_t crc_chk = Soft_Crc16_Calc(soft_crc16_default, buffer, buf_len - 2);
    return (uint8_t)(crc_rx == crc_chk);
}

void General_Data_To_Buffer(const General_data* data, uint8_t* buffer) {
    if (data == NULL || buffer == NULL) { return; }
    buffer[0] = data->len;
    memcpy(buffer + 1, data->data, data->len);
    memcpy(buffer + 1 + data->len, &data->crc16, 2);
}

void General_Data_From_Buffer(const uint8_t* buffer, General_data* data) {
    if (data == NULL || buffer == NULL) { return; }
    data->len = buffer[0];
    memcpy(data->data, buffer + 1, data->len);
    memcpy(&data->crc16, buffer + 1 + data->len, 2);
}
