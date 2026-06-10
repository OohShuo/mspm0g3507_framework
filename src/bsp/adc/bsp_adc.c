#include "bsp_adc.h"

#include <stdint.h>

#include "board_config.h"
#include "devices/msp/m0p/mspm0g350x.h"
#include "dl_adc12.h"
#include "dl_dma.h"
#include "freertos_alloc.h"
#include "vector.h"

#define ADC_DMA_BUFFER_SIZE 32

#if ADC_NUM

struct Bsp_adc_instance_t {
    ADC12_Regs* inst;
    uint32_t channel_num;
    IRQn_Type int_irqn;
    uint32_t dma_channel;
    uint32_t dma_tx_size;
    Vector* cb_dma_done_vec;
    Vector* cb_dma_done_arg_vec;
    uint32_t dma_buffer[ADC_DMA_BUFFER_SIZE];
    float* adc_value;
    float* adc_voltage;
};

static struct Bsp_adc_instance_t bsp_adc_instances[ADC_NUM] = {0};

void Bsp_Adc_Init(void) {
    for (uint32_t i = 0; i < ADC_NUM; i++) {
        bsp_adc_instances[i].inst = ((ADC12_Regs*[])ADC_PORTS)[i];
        bsp_adc_instances[i].channel_num = ((uint32_t[])ADC_CHANNEL_NUMS)[i];
        bsp_adc_instances[i].int_irqn = ((IRQn_Type[])ADC_INT_IRQNS)[i];
        bsp_adc_instances[i].dma_channel = ((uint32_t[])ADC_DMA_CHANNELS)[i];
        bsp_adc_instances[i].dma_tx_size = ((uint32_t[])ADC_DMA_TX_SIZES)[i];
        bsp_adc_instances[i].cb_dma_done_vec = Vector_Init(sizeof(Bsp_adc_cb_t), 1);
        bsp_adc_instances[i].cb_dma_done_arg_vec = Vector_Init(sizeof(void*), 1);
        bsp_adc_instances[i].adc_value =
            (float*)pvPortMalloc(sizeof(float) * bsp_adc_instances[i].channel_num);
        bsp_adc_instances[i].adc_voltage =
            (float*)pvPortMalloc(sizeof(float) * bsp_adc_instances[i].channel_num);

        DL_DMA_setSrcAddr(
            DMA, bsp_adc_instances[i].dma_channel, DL_ADC12_getFIFOAddress(bsp_adc_instances[i].inst));
        DL_DMA_setDestAddr(
            DMA, bsp_adc_instances[i].dma_channel, (uint32_t)(&bsp_adc_instances[i].dma_buffer));
        DL_DMA_enableChannel(DMA, bsp_adc_instances[i].dma_channel);
        NVIC_EnableIRQ(bsp_adc_instances[i].int_irqn);
    }
}

void Bsp_Adc_Start(uint32_t idx) {
    if (idx >= ADC_NUM) return;

    DL_ADC12_startConversion(bsp_adc_instances[idx].inst);
}

void Bsp_Adc_Stop(uint32_t idx) {
    if (idx >= ADC_NUM) return;

    DL_ADC12_stopConversion(bsp_adc_instances[idx].inst);
}

float Bsp_Adc_Read_Voltage(uint32_t idx, uint32_t channel) {
    if (idx >= ADC_NUM) return 0.0f;

    if (channel >= bsp_adc_instances[idx].channel_num) return 0.0f;

    return bsp_adc_instances[idx].adc_voltage[channel];
}

void Bsp_Adc_Register_Cb_Dma_Done(uint32_t idx, Bsp_adc_cb_t cb, void* cb_arg) {
    if (idx >= ADC_NUM || cb == NULL) return;

    Vector_Push_Back(bsp_adc_instances[idx].cb_dma_done_vec, (void*)&cb);
    Vector_Push_Back(bsp_adc_instances[idx].cb_dma_done_arg_vec, (void*)&cb_arg);
}

void Bsp_Adc_Irq_Handler(ADC12_Regs* adc_inst) {
    for (uint32_t i = 0; i < ADC_NUM; i++) {
        struct Bsp_adc_instance_t* adc = &bsp_adc_instances[i];

        if (adc->inst != adc_inst) continue;

        if (DL_ADC12_getPendingInterrupt(adc->inst) == DL_ADC12_IIDX_DMA_DONE) {
            for (uint32_t j = 0; j < adc->channel_num; j++) { adc->adc_value[j] = 0; }

            uint8_t frame_size = (adc->channel_num + 1) / 2;
            uint8_t frame_num = adc->dma_tx_size / frame_size;

            uint8_t idx = 0;
            for (uint32_t j = 0; j < frame_num; j++) {
                for (uint32_t k = 0; k < adc->channel_num; k++) {
                    uint32_t value = adc->dma_buffer[idx + (k / 2)];
                    adc->adc_value[k] += (k % 2 == 0) ? (value & 0xFFF) : ((value >> 16) & 0xFFF);
                }
                idx += frame_size;
            }

            for (uint32_t j = 0; j < adc->channel_num; j++) {
                adc->adc_value[j] /= frame_num;
                adc->adc_voltage[j] = ((float)adc->adc_value[j] / (ADC_RESOLUTION)) * ADC_REF_VOLTAGE;
            }

            for (uint32_t j = 0; j < Vector_Get_Size(adc->cb_dma_done_vec); j++) {
                Bsp_adc_cb_t cb = *(Bsp_adc_cb_t*)Vector_Get_At(adc->cb_dma_done_vec, j);
                void* cb_arg = *(void**)Vector_Get_At(adc->cb_dma_done_arg_vec, j);
                cb(cb_arg);
            }
        }
    }
}

#else

void Bsp_Adc_Init(void) {}
void Bsp_Adc_Start(uint32_t idx) { (void)idx; }
void Bsp_Adc_Stop(uint32_t idx) { (void)idx; }
void Bsp_Adc_Register_Cb_Dma_Done(uint32_t idx, Bsp_adc_cb_t cb, void* cb_arg) {
    (void)idx;
    (void)cb;
    (void)cb_arg;
}
void Bsp_Adc_Irq_Handler(ADC12_Regs* adc_inst) { (void)adc_inst; }

#endif
