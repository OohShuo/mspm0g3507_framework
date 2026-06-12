#include "w5500_hal.h"

#include <stddef.h>

#include "board_config.h"
#include "bsp_gpio.h"
#include "bsp_spi.h"
#include "cmsis_gcc.h"
#include "wizchip_conf.h"

/* ------------------------------------------------------------------ */
/* Singleton (one W5500 per build, same pattern as W25q32)              */
/* ------------------------------------------------------------------ */

static Wiz5500  g_wiz_storage;
static Wiz5500* g_wiz = NULL;

/* ------------------------------------------------------------------ */
/* Callbacks — dispatched via the singleton                            */
/* ------------------------------------------------------------------ */

static void cs_sel(void) {
    Bsp_Gpio_Write(g_wiz->config.cs_gpio_idx, bsp_gpio_state_reset);
}
static void cs_desel(void) {
    Bsp_Gpio_Write(g_wiz->config.cs_gpio_idx, bsp_gpio_state_set);
}

static uint8_t spi_rb(void) {
    uint8_t rx;
    Bsp_Hard_Spi_Read_Blocking(g_wiz->config.spi_idx, &rx, 1);
    return rx;
}
static void spi_wb(uint8_t wb) {
    Bsp_Hard_Spi_Write_Blocking(g_wiz->config.spi_idx, &wb, 1);
}

static void spi_rburst(uint8_t* buf, uint16_t len) {
    Bsp_Hard_Spi_Read_Blocking(g_wiz->config.spi_idx, buf, len);
}
static void spi_wburst(uint8_t* buf, uint16_t len) {
    Bsp_Hard_Spi_Write_Blocking(g_wiz->config.spi_idx, buf, len);
}

static void cris_en(void) {
    if (g_wiz->config.spi_mutex)
        xSemaphoreTake(g_wiz->config.spi_mutex, portMAX_DELAY);
}
static void cris_ex(void) {
    if (g_wiz->config.spi_mutex)
        xSemaphoreGive(g_wiz->config.spi_mutex);
}

/* ------------------------------------------------------------------ */
/* Public API                                                           */
/* ------------------------------------------------------------------ */

Wiz5500* W5500_Create(const W5500_Config* cfg) {
    if (cfg == NULL || g_wiz != NULL) return NULL;

    g_wiz_storage.config = *cfg;
    g_wiz_storage.inited = 0;
    g_wiz = &g_wiz_storage;

    /* ensure CS starts deselected */
    Bsp_Gpio_Write(cfg->cs_gpio_idx, bsp_gpio_state_set);

    reg_wizchip_cs_cbfunc(cs_sel, cs_desel);
    reg_wizchip_spi_cbfunc(spi_rb, spi_wb);
    reg_wizchip_spiburst_cbfunc(spi_rburst, spi_wburst);
    reg_wizchip_cris_cbfunc(cris_en, cris_ex);

    g_wiz->inited = 1;
    return g_wiz;
}

void W5500_Reset(Wiz5500* obj) {
    if (obj == NULL || obj->config.rst_gpio_idx == (uint32_t)-1) return;

    Bsp_Gpio_Write(obj->config.rst_gpio_idx, bsp_gpio_state_reset);
    for (volatile uint32_t i = 0; i < 32000; i++) __NOP();
    Bsp_Gpio_Write(obj->config.rst_gpio_idx, bsp_gpio_state_set);
    for (volatile uint32_t i = 0; i < 32000; i++) __NOP();
}

SemaphoreHandle_t W5500_Get_Mutex(Wiz5500* obj) {
    return (obj != NULL) ? obj->config.spi_mutex : NULL;
}
