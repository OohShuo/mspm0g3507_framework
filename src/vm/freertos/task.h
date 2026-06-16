#pragma once
#include "FreeRTOS.h"
BaseType_t xTaskCreate(TaskFunction_t, const char*, unsigned short, void*, unsigned int, TaskHandle_t*);
TickType_t xTaskGetTickCount(void);
void vTaskDelayUntil(TickType_t*, TickType_t);
void vTaskDelay(TickType_t);
unsigned int uxTaskGetStackHighWaterMark(TaskHandle_t);
