#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>

#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"

#define MAX_TASKS 16
typedef struct {
    TaskFunction_t f;
    void* p;
    SDL_Thread* t;
    int used;
} VmTask;
static VmTask g_tasks[MAX_TASKS];
static int g_n = 0;

static int wrap(void* a) {
    VmTask* td = (VmTask*)a;
    td->f(td->p);
    return 0;
}

BaseType_t xTaskCreate(
    TaskFunction_t f, const char* nm, unsigned short ss, void* p, unsigned int pri, TaskHandle_t* h) {
    (void)nm;
    (void)ss;
    (void)pri;
    if (g_n >= MAX_TASKS) return pdFAIL;
    VmTask* td = &g_tasks[g_n++];
    td->f = f;
    td->p = p;
    td->used = 1;
    td->t = SDL_CreateThread(wrap, nm != NULL ? nm : "vm", td);
    if (td->t == NULL) {
        td->used = 0;
        return pdFAIL;
    }
    SDL_DetachThread(td->t);
    if (h) *h = (TaskHandle_t)td;
    return pdPASS;
}
TickType_t xTaskGetTickCount(void) { return (TickType_t)SDL_GetTicks(); }
void vTaskDelayUntil(TickType_t* prev, TickType_t inc) {
    TickType_t now = (TickType_t)SDL_GetTicks(), target = *prev + inc;
    if (target > now) SDL_Delay((unsigned)(target - now));
    *prev = target;
}
void vTaskDelay(TickType_t t) {
    if (t) SDL_Delay((unsigned)t);
}
unsigned uxTaskGetStackHighWaterMark(TaskHandle_t x) {
    (void)x;
    return 4096;
}

void* pvPortMalloc(size_t sz) { return malloc(sz); }
void vPortFree(void* p) { free(p); }
size_t xPortGetFreeHeapSize(void) { return 256 * 1024 * 1024; }
size_t xPortGetMinimumEverFreeHeapSize(void) { return 200 * 1024 * 1024; }

SemaphoreHandle_t xSemaphoreCreateMutex(void) {
    return (SemaphoreHandle_t)SDL_CreateMutex();
}
BaseType_t xSemaphoreTake(SemaphoreHandle_t m, TickType_t t) {
    (void)t;
    if (m) SDL_LockMutex((SDL_mutex*)m);
    return pdPASS;
}
BaseType_t xSemaphoreGive(SemaphoreHandle_t m) {
    if (m) SDL_UnlockMutex((SDL_mutex*)m);
    return pdPASS;
}

QueueHandle_t xQueueCreate(unsigned l, unsigned s) {
    (void)l;
    (void)s;
    return NULL;
}
BaseType_t xQueueSendToBackFromISR(QueueHandle_t q, const void* i, BaseType_t* w) {
    (void)q;
    (void)i;
    (void)w;
    return pdFAIL;
}
BaseType_t xQueueReceive(QueueHandle_t q, void* b, TickType_t t) {
    (void)q;
    (void)b;
    (void)t;
    return pdFAIL;
}
