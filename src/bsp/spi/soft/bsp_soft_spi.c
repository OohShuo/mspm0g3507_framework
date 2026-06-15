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

static inline void pulse_sclk(GPIO_Regs* sclk_port, uint32_t sclk_pin) {
    sclk_port->DOUTCLR31_0 = sclk_pin;
    sclk_port->DOUTSET31_0 = sclk_pin;
}

static inline void pulse_sclk_8(GPIO_Regs* sclk_port, uint32_t sclk_pin) {
    pulse_sclk(sclk_port, sclk_pin);
    pulse_sclk(sclk_port, sclk_pin);
    pulse_sclk(sclk_port, sclk_pin);
    pulse_sclk(sclk_port, sclk_pin);
    pulse_sclk(sclk_port, sclk_pin);
    pulse_sclk(sclk_port, sclk_pin);
    pulse_sclk(sclk_port, sclk_pin);
    pulse_sclk(sclk_port, sclk_pin);
}

static inline void pulse_sclk_16(GPIO_Regs* sclk_port, uint32_t sclk_pin) {
    pulse_sclk_8(sclk_port, sclk_pin);
    pulse_sclk_8(sclk_port, sclk_pin);
}

static inline void write_byte_fast(const struct Bsp_soft_spi_instance_t* spi, uint8_t byte) {
    if (byte == 0x00u) {
        spi->mosi_port->DOUTCLR31_0 = spi->mosi_pin;
        pulse_sclk_8(spi->sclk_port, spi->sclk_pin);
        return;
    }
    if (byte == 0xffu) {
        spi->mosi_port->DOUTSET31_0 = spi->mosi_pin;
        pulse_sclk_8(spi->sclk_port, spi->sclk_pin);
        return;
    }

    for (uint8_t mask = 0x80u; mask != 0u; mask >>= 1u) {
        spi->sclk_port->DOUTCLR31_0 = spi->sclk_pin;
        if ((byte & mask) != 0u) {
            spi->mosi_port->DOUTSET31_0 = spi->mosi_pin;
        } else {
            spi->mosi_port->DOUTCLR31_0 = spi->mosi_pin;
        }
        spi->sclk_port->DOUTSET31_0 = spi->sclk_pin;
    }
}

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
        write_byte_fast(spi, data[i]);
    }
}

void Bsp_Soft_Spi_Write_Swapped16(uint32_t idx, const uint8_t* data, uint32_t len) {
    if (idx >= SOFT_SPI_NUM) { return; }
    const struct Bsp_soft_spi_instance_t* spi = &bsp_soft_spi_instances[idx];

    const uint32_t even_len = len & ~1u;
    for (uint32_t i = 0; i < even_len; i += 2u) {
        const uint16_t pixel = (uint16_t)data[i] | ((uint16_t)data[i + 1u] << 8);
        if (pixel == 0x0000u) {
            spi->mosi_port->DOUTCLR31_0 = spi->mosi_pin;
            pulse_sclk_16(spi->sclk_port, spi->sclk_pin);
            continue;
        }
        if (pixel == 0xffffu) {
            spi->mosi_port->DOUTSET31_0 = spi->mosi_pin;
            pulse_sclk_16(spi->sclk_port, spi->sclk_pin);
            continue;
        }
        write_byte_fast(spi, data[i + 1u]);
        write_byte_fast(spi, data[i]);
    }
    if (even_len != len) {
        write_byte_fast(spi, data[even_len]);
    }
}

#else

void Bsp_Soft_Spi_Init(void) {}

void Bsp_Soft_Spi_Write(uint32_t idx, const uint8_t* data, uint32_t len) {
    (void)idx;
    (void)data;
    (void)len;
}

void Bsp_Soft_Spi_Write_Swapped16(uint32_t idx, const uint8_t* data, uint32_t len) {
    (void)idx;
    (void)data;
    (void)len;
}

#endif
