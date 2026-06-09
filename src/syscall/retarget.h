#pragma once

/**
 * @brief Initialize the C runtime retarget layer.
 *
 * Call this once at the very start of main(), before any code path that
 * may produce printf output. Currently just brings up the SEGGER RTT
 * control block so early log lines (including ones from
 * vApplicationMallocFailedHook / vApplicationStackOverflowHook) reach
 * the host RTT viewer.
 */
void Syscall_Init(void);

/**
 * @file   rtt_log.h
 * @brief  See rtt_log.h in the same directory for the printf -> RTT macro.
 *         This header re-includes it for convenience so files that only
 *         need logging + Syscall_Init can include just retarget.h.
 */
#include "rtt_log.h"
