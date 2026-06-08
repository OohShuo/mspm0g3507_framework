#pragma once

#ifndef BSP_LOG_RTT_BUFFER_INDEX
    #define BSP_LOG_RTT_BUFFER_INDEX 0
#endif

void Bsp_Log_Init(void);

int bsp_printf(const char* fmt, ...)  // NOLINT(readability-identifier-naming)
    __attribute__((format(printf, 1, 2)));
