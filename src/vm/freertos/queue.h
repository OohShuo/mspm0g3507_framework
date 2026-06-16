#pragma once
#include "FreeRTOS.h"
typedef void* QueueHandle_t;
QueueHandle_t xQueueCreate(unsigned int, unsigned int);
BaseType_t xQueueSendToBackFromISR(QueueHandle_t, const void*, BaseType_t*);
BaseType_t xQueueReceive(QueueHandle_t, void*, TickType_t);
