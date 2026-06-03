#include "bsp_time.h"

#include "FreeRTOS.h"
#include "task.h"

uint32_t Bsp_Get_Tick_Ms(void) { return (uint32_t)xTaskGetTickCount(); }