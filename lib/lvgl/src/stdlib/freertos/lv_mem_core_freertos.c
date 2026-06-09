/**
 * @file lv_mem_core_freertos.c
 *
 * FreeRTOS-backed implementation of LVGL's `lv_mem` core. Enabled when
 * `LV_USE_STDLIB_MALLOC == LV_STDLIB_CUSTOM` so the project's LVGL heap
 * is unified with the FreeRTOS heap (heap_4): every `lv_malloc` /
 * `lv_realloc` / `lv_free` call lands in `pvPortMalloc` / `vPortFree`.
 *
 * Why a size header:
 *   FreeRTOS exposes no `realloc` and no public way to read a block's
 *   size, so each allocation is fronted by an 8-byte (= portBYTE_ALIGNMENT
 *   on Cortex-M0+) header that records the user-visible size. The
 *   returned pointer is `header + 1` and stays naturally aligned.
 *   `lv_realloc_core` reads the header to know how much to copy.
 *
 * Why CUSTOM and not RTTHREAD/CLIB:
 *   LVGL only ships RT-Thread, MicroPython, UEFI and clib backends.
 *   newlib's `malloc` would go through `_sbrk`, which `src/syscall/retarget.c`
 *   does not implement against the FreeRTOS heap. CUSTOM is the
 *   documented hook for project-supplied `lv_*_core` functions.
 */

/*********************
 *      INCLUDES
 *********************/
#include "../lv_mem.h"
#if LV_USE_STDLIB_MALLOC == LV_STDLIB_CUSTOM

#include "../lv_string.h"

#include "FreeRTOS.h"
#include "portable.h"
#include "task.h"

/*********************
 *      DEFINES
 *********************/

/* Header lives in front of every block so realloc/free can recover the
 * user-visible size. Must be a multiple of portBYTE_ALIGNMENT so the
 * pointer handed to LVGL stays aligned for the worst-case load. */
#define HEADER_SIZE  (((sizeof(size_t) + (portBYTE_ALIGNMENT - 1)) / portBYTE_ALIGNMENT) * portBYTE_ALIGNMENT)

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void lv_mem_init(void)
{
    /* FreeRTOS heap_4 self-initializes on the first pvPortMalloc; nothing to do. */
}

void lv_mem_deinit(void)
{
    /* Heap is owned by FreeRTOS for the lifetime of the program. */
}

lv_mem_pool_t lv_mem_add_pool(void * mem, size_t bytes)
{
    /* heap_4 is a single fixed-size pool; extra pools not supported. */
    LV_UNUSED(mem);
    LV_UNUSED(bytes);
    return NULL;
}

void lv_mem_remove_pool(lv_mem_pool_t pool)
{
    LV_UNUSED(pool);
}

void * lv_malloc_core(size_t size)
{
    if(size == 0) return NULL;

    uint8_t * raw = pvPortMalloc(size + HEADER_SIZE);
    if(raw == NULL) return NULL;

    /* Stash the user-visible size at the start of the block. */
    *((size_t *)raw) = size;
    return raw + HEADER_SIZE;
}

void * lv_realloc_core(void * p, size_t new_size)
{
    if(p == NULL) return lv_malloc_core(new_size);
    if(new_size == 0) {
        /* Match the C realloc(p, 0) "free and return NULL" contract; the
         * lv_realloc wrapper has already routed the zero-size case to the
         * sentinel before calling us, but stay safe in case it ever changes. */
        uint8_t * raw = (uint8_t *)p - HEADER_SIZE;
        vPortFree(raw);
        return NULL;
    }

    uint8_t * raw = (uint8_t *)p - HEADER_SIZE;
    size_t old_size = *((size_t *)raw);

    /* heap_4 has no in-place grow; only return p when shrinking. */
    if(new_size <= old_size) {
        *((size_t *)raw) = new_size;
        return p;
    }

    void * new_p = lv_malloc_core(new_size);
    if(new_p == NULL) return NULL;

    lv_memcpy(new_p, p, old_size);
    vPortFree(raw);
    return new_p;
}

void lv_free_core(void * p)
{
    if(p == NULL) return;
    uint8_t * raw = (uint8_t *)p - HEADER_SIZE;
    vPortFree(raw);
}

void lv_mem_monitor_core(lv_mem_monitor_t * mon_p)
{
    /* heap_4 doesn't expose a free-block list, so free_cnt / used_cnt /
     * free_biggest_size stay at the lv_memzero'd 0 the caller set up.
     * The numbers we *can* honestly report are total, currently-free, and
     * the high-water mark from xPortGetMinimumEverFreeHeapSize. */
    const size_t total = configTOTAL_HEAP_SIZE;
    const size_t free_now = xPortGetFreeHeapSize();
    const size_t free_low = xPortGetMinimumEverFreeHeapSize();

    mon_p->total_size = total;
    mon_p->free_size = free_now;
    mon_p->max_used = total - free_low;
    mon_p->used_pct = (uint8_t)(((total - free_now) * 100U) / total);
    mon_p->frag_pct = 0; /* Not derivable from FreeRTOS heap_4 public API. */
}

lv_result_t lv_mem_test_core(void)
{
    /* No corruption detector in heap_4; rely on configASSERT + the
     * vApplicationMallocFailedHook wired up in src/it.c. */
    return LV_RESULT_OK;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

#endif /*LV_USE_STDLIB_MALLOC == LV_STDLIB_CUSTOM*/
