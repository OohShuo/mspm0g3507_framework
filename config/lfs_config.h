#pragma once

#include "SEGGER_RTT.h"

#define LFS_NO_MALLOC

#define LFS_TRACE(fmt, ...) SEGGER_RTT_printf(0, "%s:%d:trace: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)

#define LFS_DEBUG(fmt, ...) SEGGER_RTT_printf(0, "%s:%d:debug: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)

#define LFS_WARN(fmt, ...)  SEGGER_RTT_printf(0, "%s:%d:warn: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)

#define LFS_ERROR(fmt, ...) SEGGER_RTT_printf(0, "%s:%d:error: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)
