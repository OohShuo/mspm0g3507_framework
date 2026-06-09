#include "bsp_soft_spi.h"

#include "board_config.h"
#include "devices/msp/m0p/mspm0g350x.h"
#include "dl_gpio.h"

#if SOFT_SPI_NUM

struct Bsp_soft_spi_instance_t {
    GPIO_Regs* sclk_port;
    uint32_t sclk_pin;
    GPIO_Regs* mosi_port;
    uint32_t mosi_pin;
};

static struct Bsp_soft_spi_instance_t bsp_soft_spi_instances[SOFT_SPI_NUM] = {0};

void Bsp_Soft_Spi_Init(void) {
    for (uint32_t i = 0; i < SOFT_SPI_NUM; i++) {
        bsp_soft_spi_instances[i].sclk_port = ((GPIO_Regs*[])SOFT_SPI_SCLK_PORTS)[i];
        bsp_soft_spi_instances[i].sclk_pin = ((uint32_t[])SOFT_SPI_SCLK_PINS)[i];
        bsp_soft_spi_instances[i].mosi_port = ((GPIO_Regs*[])SOFT_SPI_MOSI_PORTS)[i];
        bsp_soft_spi_instances[i].mosi_pin = ((uint32_t[])SOFT_SPI_MOSI_PINS)[i];
    }
}

void Bsp_Soft_Spi_Write(uint32_t idx, const uint8_t* data, uint32_t len) {
    if (idx >= SOFT_SPI_NUM) { return; }
    const struct Bsp_soft_spi_instance_t* spi = &bsp_soft_spi_instances[idx];

    for (uint32_t i = 0; i < len; i++) {
        uint8_t byte = data[i];
        for (uint8_t j = 0; j < 8; j++) {
            DL_GPIO_clearPins(spi->sclk_port, spi->sclk_pin);
            if (byte & 0x80) {
                DL_GPIO_setPins(spi->mosi_port, spi->mosi_pin);
            } else {
                DL_GPIO_clearPins(spi->mosi_port, spi->mosi_pin);
            }
            DL_GPIO_setPins(spi->sclk_port, spi->sclk_pin);
            byte = (uint8_t)(byte << 1);
        }
    }
}

#else

void Bsp_Soft_Spi_Init(void) {}

void Bsp_Soft_Spi_Write(uint32_t idx, const uint8_t* data, uint32_t len) {
    (void)idx;
    (void)data;
    (void)len;
}

#endif
