/**
 * @file   rtt_log.h
 * @brief  Map printf() -> SEGGER_RTT_printf() at the preprocessor level.
 *
 * Any .c file that wants to use printf to log to RTT should include
 * this header INSTEAD OF <stdio.h>. After the include, every
 * `printf(fmt, ...)` call expands to `SEGGER_RTT_printf(0, fmt, ...)`,
 * which bypasses newlib's FILE-buffer machinery entirely — no _write,
 * no fputc, no buffering mode debate.
 *
 * Why this beats newlib retarget on this target:
 *   - newlib-nano + nosys `_isatty` returning -1 leaves stdout in fully
 *     buffered mode. Short log lines sit in the FILE buffer and never
 *     reach RTT. The conventional fixes (setvbuf, override _isatty,
 *     override fputc) all depend on newlib's internal state machine
 *     behaving the way you think it does on a given build.
 *   - SEGGER_RTT_printf formats and writes in one shot, with no
 *     intermediate buffer, no flush decision, no mode query. It just
 *     goes. Predictable, debuggable, and zero newlib stdio surface.
 *
 * Caveats:
 *   - The `printf` macro is a function-like macro, so `&printf` or
 *     passing printf as a function pointer won't work. Nothing in this
 *     project does that, but library code that does will miscompile.
 *   - `fprintf` / `sprintf` / `snprintf` are NOT remapped. Use
 *     SEGGER_RTT_printf directly for those, or add a similar macro
 *     here if you need them.
 */

#pragma once

#include "SEGGER_RTT.h"

/* RTT up-buffer 0 is the standard "terminal" channel. */
#define printf(...) SEGGER_RTT_printf(0, __VA_ARGS__)
