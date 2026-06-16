#pragma once

/**
 * @brief Initialize the C runtime retarget layer.
 *
 * When RTT is enabled, brings up the SEGGER RTT control block so early
 * log lines reach the host RTT viewer.
 */
void Syscall_Init(void);

#if FRAMEWORK_USE_RTT
    /**
     * @file   rtt_log.h
     * @brief  See rtt_log.h in the same directory for the printf -> RTT macro.
     *         This header re-includes it for convenience so files that only
     *         need logging + Syscall_Init can include just retarget.h.
     */
    #include "rtt_log.h"
#endif
