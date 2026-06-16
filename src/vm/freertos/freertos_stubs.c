#include <SDL2/SDL.h>
#include <pthread.h>
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
    pthread_t t;
    int used;
} VmTask;
static VmTask g_tasks[MAX_TASKS];
static int g_n = 0;

static void* wrap(void* a) {
    VmTask* td = (VmTask*)a;
    td->f(td->p);
    return NULL;
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
    if (pthread_create(&td->t, NULL, wrap, td) != 0) {
        td->used = 0;
        return pdFAIL;
    }
    pthread_detach(td->t);
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
    pthread_mutex_t* m = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
    if (m) pthread_mutex_init(m, NULL);
    return (SemaphoreHandle_t)m;
}
BaseType_t xSemaphoreTake(SemaphoreHandle_t m, TickType_t t) {
    (void)t;
    if (m) pthread_mutex_lock(m);
    return pdPASS;
}
BaseType_t xSemaphoreGive(SemaphoreHandle_t m) {
    if (m) pthread_mutex_unlock(m);
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
