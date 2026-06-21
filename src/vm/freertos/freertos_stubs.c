#include <SDL2/SDL.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"
#include "task_vm.h"

#define MAX_TASKS 16
typedef struct {
    TaskFunction_t f;
    void* p;
    pthread_t t;
    int used;
} VmTask;
static VmTask g_tasks[MAX_TASKS];
static int g_n = 0;
static int g_started = 0;
static pthread_mutex_t g_tasks_mutex = PTHREAD_MUTEX_INITIALIZER;

static void* wrap(void* a) {
    VmTask* td = (VmTask*)a;
    td->f(td->p);
    return NULL;
}

static BaseType_t start_task(VmTask* task) {
    if (pthread_create(&task->t, NULL, wrap, task) != 0) return pdFAIL;
    pthread_detach(task->t);
    return pdPASS;
}

BaseType_t xTaskCreate(
    TaskFunction_t f, const char* nm, unsigned short ss, void* p, unsigned int pri, TaskHandle_t* h) {
    (void)nm;
    (void)ss;
    (void)pri;
    pthread_mutex_lock(&g_tasks_mutex);
    if (g_n >= MAX_TASKS) {
        pthread_mutex_unlock(&g_tasks_mutex);
        return pdFAIL;
    }
    VmTask* td = &g_tasks[g_n++];
    td->f = f;
    td->p = p;
    td->used = 1;
    if (g_started && start_task(td) != pdPASS) {
        td->used = 0;
        g_n--;
        pthread_mutex_unlock(&g_tasks_mutex);
        return pdFAIL;
    }
    if (h) *h = (TaskHandle_t)td;
    pthread_mutex_unlock(&g_tasks_mutex);
    return pdPASS;
}

BaseType_t Vm_Freertos_Start_Tasks(void) {
    BaseType_t result = pdPASS;
    pthread_mutex_lock(&g_tasks_mutex);
    if (!g_started) {
        g_started = 1;
        for (int i = 0; i < g_n; ++i) {
            if (g_tasks[i].used && start_task(&g_tasks[i]) != pdPASS) {
                g_tasks[i].used = 0;
                result = pdFAIL;
            }
        }
    }
    pthread_mutex_unlock(&g_tasks_mutex);
    return result;
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
