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
