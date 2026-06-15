#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

// Let FreeRTOSConfig.h define configASSERT first (avoids redefinition warning),
// then override with a version that always evaluates its argument (no NDEBUG).
#include "FreeRTOSConfig.h"
#undef  configASSERT
#define configASSERT(x) do { if (!(x)) { \
    fprintf(stderr, "ASSERT %s:%d: %s\n", __FILE__, __LINE__, #x); abort(); \
} } while(0)
#define pdTRUE          1
#define pdFALSE         0
#define pdPASS          1
#define pdFAIL          0
#define pdMS_TO_TICKS(ms) ((TickType_t)(ms))
#define portTICK_PERIOD_MS 1
#define portMAX_DELAY       ((TickType_t)0xFFFFFFFF)
typedef uint32_t TickType_t;
typedef int BaseType_t;
typedef unsigned int UBaseType_t;
typedef unsigned long StackType_t;
typedef void (*TaskFunction_t)(void*);
typedef void* TaskHandle_t;
#define taskENTER_CRITICAL()
#define taskEXIT_CRITICAL()
#define taskDISABLE_INTERRUPTS()
#define taskENABLE_INTERRUPTS()
#define portYIELD_FROM_ISR(x) do { (void)(x); } while(0)
size_t xPortGetFreeHeapSize(void);
size_t xPortGetMinimumEverFreeHeapSize(void);
void* pvPortMalloc(size_t xSize);
void vPortFree(void* pv);
