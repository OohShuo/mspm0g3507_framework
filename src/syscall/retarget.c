/**
 * @file    retarget.c
 * @brief   Project-global C runtime retarget for newlib-nano.
 *
 * Why this lives in src/syscall/ and not src/bsp/:
 *   These hooks are what *every* translation unit in the project
 *   (application code, LVGL, LFS, FreeRTOS hooks, ...) ultimately
 *   resolves printf / scanf / malloc against. Putting them in bsp
 *   would force external libraries to link the board layer just to
 *   print a log line — a layering violation. syscall/ is the
 *   "everyone needs it, the board doesn't matter" layer.
 *
 * Linker contract:
 *   Root CMakeLists.txt links `--specs=nano.specs --specs=nosys.specs`
 *   and this translation unit. The nosys spec provides WEAK stubs for
 *   every syscall; the three below are STRONG and override them. The
 *   rest (_close, _fstat, _isatty, _lseek, _getpid, _kill, _exit, ...)
 *   stay as the weak nosys defaults and are dropped by --gc-sections
 *   when nothing references them.
 */

#include "retarget.h"

#include <errno.h>
#include <stddef.h>
#include <unistd.h>

#include "SEGGER_RTT.h"

/* RTT up-buffer 0 is the standard "terminal" channel. */
#define LOG_RTT_BUF_IDX 0

/* ------------------------------------------------------------------ */
/* Public init                                                         */
/* ------------------------------------------------------------------ */

void Syscall_Init(void) { SEGGER_RTT_Init(); }

/* ------------------------------------------------------------------ */
/* _write — printf / puts / fwrite(stdout) exit point                  */
/* ------------------------------------------------------------------ */

int _write(int fd, const void *buf, size_t count) {
    /* newlib may route any fd through here; only stdout/stderr go to RTT. */
    if (fd != STDOUT_FILENO && fd != STDERR_FILENO) {
        return -1;
    }
    if (count == 0) {
        return 0;
    }
    /* SEGGER_RTT_Write is IRQ-safe on Cortex-M0+ (uses PRIMASK internally)
     * and respects BUFFER_SIZE_UP / RTT mode config for overflow. */
    SEGGER_RTT_Write(LOG_RTT_BUF_IDX, buf, count);
    return (int)count;
}

/* ------------------------------------------------------------------ */
/* _read — scanf / getchar entry. We have no host->target input.       */
/* ------------------------------------------------------------------ */

int _read(int fd, void *buf, size_t count) {
    (void)fd;
    (void)buf;
    (void)count;
    return 0; /* EOF — scanf on this target is a no-op */
}

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
