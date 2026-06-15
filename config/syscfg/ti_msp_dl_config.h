/*
 * Copyright (c) 2023, Texas Instruments Incorporated - http://www.ti.com
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *  ============ ti_msp_dl_config.h =============
 *  Configured MSPM0 DriverLib module declarations
 *
 *  DO NOT EDIT - This file is generated for the MSPM0G350X
 *  by the SysConfig tool.
 */
#ifndef ti_msp_dl_config_h
#define ti_msp_dl_config_h

#define CONFIG_MSPM0G350X
#define CONFIG_MSPM0G3507

#if defined(__ti_version__) || defined(__TI_COMPILER_VERSION__)
#define SYSCONFIG_WEAK __attribute__((weak))
#elif defined(__IAR_SYSTEMS_ICC__)
#define SYSCONFIG_WEAK __weak
#elif defined(__GNUC__)
#define SYSCONFIG_WEAK __attribute__((weak))
#endif

#include <ti/devices/msp/msp.h>
#include <ti/driverlib/driverlib.h>
#include <ti/driverlib/m0p/dl_core.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 *  ======== SYSCFG_DL_init ========
 *  Perform all required MSP DL initialization
 *
 *  This function should be called once at a point before any use of
 *  MSP DL.
 */


/* clang-format off */

#define POWER_STARTUP_DELAY                                                (16)


#define GPIO_HFXT_PORT                                                     GPIOA
#define GPIO_HFXIN_PIN                                             DL_GPIO_PIN_5
#define GPIO_HFXIN_IOMUX                                         (IOMUX_PINCM10)
#define GPIO_HFXOUT_PIN                                            DL_GPIO_PIN_6
#define GPIO_HFXOUT_IOMUX                                        (IOMUX_PINCM11)
#define CPUCLK_FREQ                                                     80000000
/* Defines for SYSPLL_ERR_01 Workaround */
/* Represent 1.000 as 1000 */
#define FLOAT_TO_INT_SCALE                                               (1000U)
#define FCC_EXPECTED_RATIO                                                  2000
#define FCC_UPPER_BOUND                       (FCC_EXPECTED_RATIO * (1 + 0.003))
#define FCC_LOWER_BOUND                       (FCC_EXPECTED_RATIO * (1 - 0.003))

bool SYSCFG_DL_SYSCTL_SYSPLL_init(void);


/* Defines for PWM_0 */
#define PWM_0_INST                                                         TIMA0
#define PWM_0_INST_IRQHandler                                   TIMA0_IRQHandler
#define PWM_0_INST_INT_IRQN                                     (TIMA0_INT_IRQn)
#define PWM_0_INST_CLK_FREQ                                              4000000
/* GPIO defines for channel 3 */
#define GPIO_PWM_0_C3_PORT                                                 GPIOA
#define GPIO_PWM_0_C3_PIN                                         DL_GPIO_PIN_12
#define GPIO_PWM_0_C3_IOMUX                                      (IOMUX_PINCM34)
#define GPIO_PWM_0_C3_IOMUX_FUNC                     IOMUX_PINCM34_PF_TIMA0_CCP3
#define GPIO_PWM_0_C3_IDX                                    DL_TIMER_CC_3_INDEX

/* Defines for PWM_1 */
#define PWM_1_INST                                                         TIMG0
#define PWM_1_INST_IRQHandler                                   TIMG0_IRQHandler
#define PWM_1_INST_INT_IRQN                                     (TIMG0_INT_IRQn)
#define PWM_1_INST_CLK_FREQ                                              4000000
/* GPIO defines for channel 1 */
#define GPIO_PWM_1_C1_PORT                                                 GPIOA
#define GPIO_PWM_1_C1_PIN                                         DL_GPIO_PIN_13
#define GPIO_PWM_1_C1_IOMUX                                      (IOMUX_PINCM35)
#define GPIO_PWM_1_C1_IOMUX_FUNC                     IOMUX_PINCM35_PF_TIMG0_CCP1
#define GPIO_PWM_1_C1_IDX                                    DL_TIMER_CC_1_INDEX



/* Defines for TIMER_0 */
#define TIMER_0_INST                                                     (TIMA1)
#define TIMER_0_INST_IRQHandler                                 TIMA1_IRQHandler
#define TIMER_0_INST_INT_IRQN                                   (TIMA1_INT_IRQn)
#define TIMER_0_INST_LOAD_VALUE                                           (999U)



/* Defines for UART_0 */
#define UART_0_INST                                                        UART0
#define UART_0_INST_FREQUENCY                                           40000000
#define UART_0_INST_IRQHandler                                  UART0_IRQHandler
#define UART_0_INST_INT_IRQN                                      UART0_INT_IRQn
#define GPIO_UART_0_RX_PORT                                                GPIOA
#define GPIO_UART_0_TX_PORT                                                GPIOA
#define GPIO_UART_0_RX_PIN                                        DL_GPIO_PIN_11
#define GPIO_UART_0_TX_PIN                                        DL_GPIO_PIN_10
#define GPIO_UART_0_IOMUX_RX                                     (IOMUX_PINCM22)
#define GPIO_UART_0_IOMUX_TX                                     (IOMUX_PINCM21)
#define GPIO_UART_0_IOMUX_RX_FUNC                      IOMUX_PINCM22_PF_UART0_RX
#define GPIO_UART_0_IOMUX_TX_FUNC                      IOMUX_PINCM21_PF_UART0_TX
#define UART_0_BAUD_RATE                                                (115200)
#define UART_0_IBRD_40_MHZ_115200_BAUD                                      (21)
#define UART_0_FBRD_40_MHZ_115200_BAUD                                      (45)




/* Defines for SPI_0 */
#define SPI_0_INST                                                         SPI1
#define SPI_0_INST_IRQHandler                                   SPI1_IRQHandler
#define SPI_0_INST_INT_IRQN                                       SPI1_INT_IRQn
#define GPIO_SPI_0_PICO_PORT                                              GPIOB
#define GPIO_SPI_0_PICO_PIN                                      DL_GPIO_PIN_15
#define GPIO_SPI_0_IOMUX_PICO                                   (IOMUX_PINCM32)
#define GPIO_SPI_0_IOMUX_PICO_FUNC                   IOMUX_PINCM32_PF_SPI1_PICO
#define GPIO_SPI_0_POCI_PORT                                              GPIOB
#define GPIO_SPI_0_POCI_PIN                                      DL_GPIO_PIN_14
#define GPIO_SPI_0_IOMUX_POCI                                   (IOMUX_PINCM31)
#define GPIO_SPI_0_IOMUX_POCI_FUNC                   IOMUX_PINCM31_PF_SPI1_POCI
/* GPIO configuration for SPI_0 */
#define GPIO_SPI_0_SCLK_PORT                                              GPIOB
#define GPIO_SPI_0_SCLK_PIN                                      DL_GPIO_PIN_16
#define GPIO_SPI_0_IOMUX_SCLK                                   (IOMUX_PINCM33)
#define GPIO_SPI_0_IOMUX_SCLK_FUNC                   IOMUX_PINCM33_PF_SPI1_SCLK
#define GPIO_SPI_0_CS0_PORT                                               GPIOA
#define GPIO_SPI_0_CS0_PIN                                        DL_GPIO_PIN_2
#define GPIO_SPI_0_IOMUX_CS0                                     (IOMUX_PINCM7)
#define GPIO_SPI_0_IOMUX_CS0_FUNC                      IOMUX_PINCM7_PF_SPI1_CS0



/* Defines for ADC12_0 */
#define ADC12_0_INST                                                        ADC0
#define ADC12_0_INST_IRQHandler                                  ADC0_IRQHandler
#define ADC12_0_INST_INT_IRQN                                    (ADC0_INT_IRQn)
#define ADC12_0_ADCMEM_0                                      DL_ADC12_MEM_IDX_0
#define ADC12_0_ADCMEM_0_REF                     DL_ADC12_REFERENCE_VOLTAGE_VDDA
#define ADC12_0_ADCMEM_0_REF_VOLTAGE_V                                       3.3
#define ADC12_0_ADCMEM_1                                      DL_ADC12_MEM_IDX_1
#define ADC12_0_ADCMEM_1_REF                     DL_ADC12_REFERENCE_VOLTAGE_VDDA
#define ADC12_0_ADCMEM_1_REF_VOLTAGE_V                                       3.3
#define GPIO_ADC12_0_C0_PORT                                               GPIOA
#define GPIO_ADC12_0_C0_PIN                                       DL_GPIO_PIN_27
#define GPIO_ADC12_0_IOMUX_C0                                    (IOMUX_PINCM60)
#define GPIO_ADC12_0_IOMUX_C0_FUNC                (IOMUX_PINCM60_PF_UNCONNECTED)
#define GPIO_ADC12_0_C1_PORT                                               GPIOA
#define GPIO_ADC12_0_C1_PIN                                       DL_GPIO_PIN_26
#define GPIO_ADC12_0_IOMUX_C1                                    (IOMUX_PINCM59)
#define GPIO_ADC12_0_IOMUX_C1_FUNC                (IOMUX_PINCM59_PF_UNCONNECTED)



/* Defines for DMA_CH0 */
#define DMA_CH0_CHAN_ID                                                      (0)
#define ADC12_0_INST_DMA_TRIGGER                      (DMA_ADC0_EVT_GEN_BD_TRIG)
/* Defines for DMA_CH1 */
#define DMA_CH1_CHAN_ID                                                      (2)
#define SPI_0_INST_DMA_TRIGGER_0                              (DMA_SPI1_RX_TRIG)
/* Defines for DMA_CH2 */
#define DMA_CH2_CHAN_ID                                                      (1)
#define SPI_0_INST_DMA_TRIGGER_1                              (DMA_SPI1_TX_TRIG)
/* Defines for DMA_CH3 */
#define DMA_CH3_CHAN_ID                                                      (4)
#define UART_0_INST_DMA_TRIGGER_0                            (DMA_UART0_RX_TRIG)
/* Defines for DMA_CH4 */
#define DMA_CH4_CHAN_ID                                                      (3)
#define UART_0_INST_DMA_TRIGGER_1                            (DMA_UART0_TX_TRIG)


/* Port definition for Pin Group GPIO_GRP_0 */
#define GPIO_GRP_0_PORT                                                  (GPIOA)

/* Defines for PIN_0: GPIOA.25 with pinCMx 55 on package pin 26 */
#define GPIO_GRP_0_PIN_0_PIN                                    (DL_GPIO_PIN_25)
#define GPIO_GRP_0_PIN_0_IOMUX                                   (IOMUX_PINCM55)
/* Defines for PIN_1: GPIOA.18 with pinCMx 40 on package pin 11 */
#define GPIO_GRP_0_PIN_1_PIN                                    (DL_GPIO_PIN_18)
#define GPIO_GRP_0_PIN_1_IOMUX                                   (IOMUX_PINCM40)
/* Port definition for Pin Group GPIO_GRP_1 */
#define GPIO_GRP_1_PORT                                                  (GPIOB)

/* Defines for PIN_4: GPIOB.2 with pinCMx 15 on package pin 50 */
#define GPIO_GRP_1_PIN_4_PIN                                     (DL_GPIO_PIN_2)
#define GPIO_GRP_1_PIN_4_IOMUX                                   (IOMUX_PINCM15)
/* Defines for PIN_5: GPIOB.18 with pinCMx 44 on package pin 15 */
#define GPIO_GRP_1_PIN_5_PIN                                    (DL_GPIO_PIN_18)
#define GPIO_GRP_1_PIN_5_IOMUX                                   (IOMUX_PINCM44)
/* Defines for PIN_6: GPIOB.19 with pinCMx 45 on package pin 16 */
#define GPIO_GRP_1_PIN_6_PIN                                    (DL_GPIO_PIN_19)
#define GPIO_GRP_1_PIN_6_IOMUX                                   (IOMUX_PINCM45)
/* Defines for PIN_7: GPIOB.20 with pinCMx 48 on package pin 19 */
#define GPIO_GRP_1_PIN_7_PIN                                    (DL_GPIO_PIN_20)
#define GPIO_GRP_1_PIN_7_IOMUX                                   (IOMUX_PINCM48)
/* Defines for PIN_8: GPIOB.24 with pinCMx 52 on package pin 23 */
#define GPIO_GRP_1_PIN_8_PIN                                    (DL_GPIO_PIN_24)
#define GPIO_GRP_1_PIN_8_IOMUX                                   (IOMUX_PINCM52)
/* Defines for PIN_9: GPIOB.17 with pinCMx 43 on package pin 14 */
#define GPIO_GRP_1_PIN_9_PIN                                    (DL_GPIO_PIN_17)
#define GPIO_GRP_1_PIN_9_IOMUX                                   (IOMUX_PINCM43)
/* Defines for PIN_SCL: GPIOA.17 with pinCMx 39 on package pin 10 */
#define GPIO_TFT_PIN_SCL_PORT                                            (GPIOA)
#define GPIO_TFT_PIN_SCL_PIN                                    (DL_GPIO_PIN_17)
#define GPIO_TFT_PIN_SCL_IOMUX                                   (IOMUX_PINCM39)
/* Defines for PIN_SDA: GPIOB.8 with pinCMx 25 on package pin 60 */
#define GPIO_TFT_PIN_SDA_PORT                                            (GPIOB)
#define GPIO_TFT_PIN_SDA_PIN                                     (DL_GPIO_PIN_8)
#define GPIO_TFT_PIN_SDA_IOMUX                                   (IOMUX_PINCM25)
/* Defines for PIN_RES: GPIOA.16 with pinCMx 38 on package pin 9 */
#define GPIO_TFT_PIN_RES_PORT                                            (GPIOA)
#define GPIO_TFT_PIN_RES_PIN                                    (DL_GPIO_PIN_16)
#define GPIO_TFT_PIN_RES_IOMUX                                   (IOMUX_PINCM38)
/* Defines for PIN_DC: GPIOA.15 with pinCMx 37 on package pin 8 */
#define GPIO_TFT_PIN_DC_PORT                                             (GPIOA)
#define GPIO_TFT_PIN_DC_PIN                                     (DL_GPIO_PIN_15)
#define GPIO_TFT_PIN_DC_IOMUX                                    (IOMUX_PINCM37)
/* Defines for PIN_BLK: GPIOA.14 with pinCMx 36 on package pin 7 */
#define GPIO_TFT_PIN_BLK_PORT                                            (GPIOA)
#define GPIO_TFT_PIN_BLK_PIN                                    (DL_GPIO_PIN_14)
#define GPIO_TFT_PIN_BLK_IOMUX                                   (IOMUX_PINCM36)




/* clang-format on */

void SYSCFG_DL_init(void);
void SYSCFG_DL_initPower(void);
void SYSCFG_DL_GPIO_init(void);
void SYSCFG_DL_SYSCTL_init(void);

bool SYSCFG_DL_SYSCTL_SYSPLL_init(void);
void SYSCFG_DL_PWM_0_init(void);
void SYSCFG_DL_PWM_1_init(void);
void SYSCFG_DL_TIMER_0_init(void);
void SYSCFG_DL_UART_0_init(void);
void SYSCFG_DL_SPI_0_init(void);
void SYSCFG_DL_ADC12_0_init(void);
void SYSCFG_DL_DMA_init(void);

void SYSCFG_DL_SYSTICK_init(void);

bool SYSCFG_DL_saveConfiguration(void);
bool SYSCFG_DL_restoreConfiguration(void);

#ifdef __cplusplus
}
#endif

#endif /* ti_msp_dl_config_h */
