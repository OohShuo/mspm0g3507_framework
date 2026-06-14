#pragma once

#define LFS_LOG_ENABLE 0

#include "SEGGER_RTT.h"

// NOLINTBEGIN(readability-identifier-naming)

extern void* pvPortMalloc(size_t xWantedSize);
extern void vPortFree(void* pv);

// NOLINTEND(readability-identifier-naming)

#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"

#define LFS_MALLOC(sz) pvPortMalloc((sz))
#define LFS_FREE(p)    vPortFree((p))
#define LFS_ASSERT(x)  configASSERT(x)

#if LFS_LOG_ENABLE
    #define LFS_TRACE(fmt, ...) \
        SEGGER_RTT_printf(0, "%s:%d:trace: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)

    #define LFS_DEBUG(fmt, ...) \
        SEGGER_RTT_printf(0, "%s:%d:debug: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)

    #define LFS_WARN(fmt, ...) \
        SEGGER_RTT_printf(0, "%s:%d:warn: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)

    #define LFS_ERROR(fmt, ...) \
        SEGGER_RTT_printf(0, "%s:%d:error: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#endif
