/**
 * @file   retarget.c
 * @brief  Project-globally linked symbols that newlib's libc still needs
 *         when the application is built against newlib-nano.
 *
 * After switching printf to the SEGGER_RTT_printf macro (see rtt_log.h),
 * newlib's stdio path is no longer used by any source in the project.
 * What is still needed:
 *
 *   - Syscall_Init: brings up the SEGGER RTT control block before any
 *     RTT write can succeed.
 *   - _sbrk: newlib's malloc / FreeRTOS heap_4 still go through this.
 *     Without a strong definition, malloc returns NULL.
 *
 * Everything else (close, fstat, isatty, lseek, getpid, kill, _exit,
 * fputc, _write, _read) is either dropped by --gc-sections (because
 * nothing references it) or satisfied by nosys's weak defaults. We
 * intentionally do NOT define _write / _read / fputc — defining them
 * when nothing uses them would just be dead code.
 */

#include "retarget.h"

#include <errno.h>
#include <stddef.h>

#include "SEGGER_RTT.h"

/* ------------------------------------------------------------------ */
/* Public init                                                         */
/* ------------------------------------------------------------------ */

void Syscall_Init(void) { SEGGER_RTT_Init(); }

/* ------------------------------------------------------------------ */
/* _sbrk — newlib heap, used by malloc / FreeRTOS heap_*.c             */
/*                                                                    */
/* Heap region is fixed by the linker script (see .heap in            */
/* ti_device/.../mspm0g3507.lds): __heap_start__ .. __HeapLimit.      */
/* ------------------------------------------------------------------ */

extern char __heap_start__[]; /* start of heap region, post-bss */
extern char __HeapLimit[];    /* first byte PAST legal heap */

void* _sbrk(ptrdiff_t incr) {
    static char* heap_end = &__heap_start__[0];

    char* prev = heap_end;
    char* next = prev + incr;

    /* Overflow guard: refuse to hand out memory past the heap ceiling. */
    if (next > &__HeapLimit[0]) {
        errno = ENOMEM;
        return (void*)-1;
    }
    heap_end = next;
    return prev;
}
