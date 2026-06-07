#pragma once

// Main application interface. Called from main.c after FreeRTOS scheduler
// starts (App_Init must be called from a task — it uses vTaskDelay).
void App_Init(void);
void App_Loop(void);
