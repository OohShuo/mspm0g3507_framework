#pragma once

#include <stdint.h>

#include "FreeRTOS.h"
#include "semphr.h"

/**
 * @brief W5500 HAL instance — follows the W25q32_Create() pattern.
 *
 * Usage:
 *   W5500* wiz = W5500_Create(&(W5500_Config){
 *       .spi_idx = SPI_LCD_IDX, .cs_gpio_idx = 10,
 *       .rst_gpio_idx = 11, .spi_mutex = g_spi_mutex,
 *   });
 *   // then pass wiz to Com_Udp_Create()
 */

typedef struct {
    uint32_t spi_idx;            /**< hardware SPI index (shared bus)          */
    uint32_t cs_gpio_idx;        /**< CS GPIO index                            */
    uint32_t rst_gpio_idx;       /**< RST GPIO index (0xFFFFFFFF if unused)     */
    SemaphoreHandle_t spi_mutex; /**< SPI bus mutex (NULL = unprotected)        */
} W5500_Config;

typedef struct {
    W5500_Config config;
    uint8_t inited;
} Wiz5500; /* renamed — "W5500" is a wiznet macro */

/**
 * @brief Create and initialise a Wiz5500 instance.
 *
 * Registers SPI/CS/critical-section callbacks with the wiznet library.
 * Does NOT call wizchip_init() — the caller (com_udp) does that.
 */
Wiz5500* W5500_Create(const W5500_Config* cfg);

void W5500_Reset(Wiz5500* obj);
SemaphoreHandle_t W5500_Get_Mutex(Wiz5500* obj);
