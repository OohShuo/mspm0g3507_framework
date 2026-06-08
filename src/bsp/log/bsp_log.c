#include "bsp_log.h"

#include <stdarg.h>

#include "SEGGER_RTT.h"

void Bsp_Log_Init(void) { SEGGER_RTT_Init(); }

int bsp_printf(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int n = SEGGER_RTT_vprintf(BSP_LOG_RTT_BUFFER_INDEX, fmt, &ap);
    va_end(ap);
    return n;
}
