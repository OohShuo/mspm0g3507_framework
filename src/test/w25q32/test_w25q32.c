#include "test_w25q32.h"

#include <string.h>

#include "FreeRTOS.h"
#include "board_config.h"
#include "bsp_gpio.h"
#include "task.h"
#include "w25q32.h"

#define W25Q32_TEST_RESULT_MAX 8
#define W25Q32_TEST_BUF_SIZE   256

typedef struct {
    const char* name;
    uint8_t passed;
    uint8_t raw;
} W25q32_test_result;

static W25q32* g_w25q32 = NULL;
static volatile uint8_t g_w25q32_jedec_ok = 0;
static uint8_t g_w25q32_test_buf[W25Q32_TEST_BUF_SIZE];
static W25q32_test_result g_w25q32_results[W25Q32_TEST_RESULT_MAX];
static uint8_t g_w25q32_result_count = 0;
static uint8_t g_w25q32_all_passed = 0;

static void w25q32_record(const char* name, uint8_t passed) {
    if (g_w25q32_result_count < W25Q32_TEST_RESULT_MAX) {
        g_w25q32_results[g_w25q32_result_count].name = name;
        g_w25q32_results[g_w25q32_result_count].passed = passed;
        g_w25q32_results[g_w25q32_result_count].raw = 0;
        g_w25q32_result_count++;
    }
}

static void w25q32_record_raw(const char* name, uint8_t passed, uint8_t raw) {
    if (g_w25q32_result_count < W25Q32_TEST_RESULT_MAX) {
        g_w25q32_results[g_w25q32_result_count].name = name;
        g_w25q32_results[g_w25q32_result_count].passed = passed;
        g_w25q32_results[g_w25q32_result_count].raw = raw;
        g_w25q32_result_count++;
    }
}

static void w25q32_init(void) {
    const W25q32_config cfg = {
        .spi_idx = SPI_LCD_IDX,
        .cs_gpio_idx = GPIO_SPI_CS_IDX,
    };
    g_w25q32 = W25q32_Create(&cfg);
    g_w25q32_jedec_ok = W25q32_Init(g_w25q32);
}

static void w25q32_loop(void) {
    if (g_w25q32 == NULL) { return; }

    g_w25q32_result_count = 0;

    uint8_t sr1 = W25q32_Read_Status_Reg_1(g_w25q32);
    w25q32_record_raw("SR1.BUSY=0", (uint8_t)((sr1 & W25Q32_SR1_BUSY) == 0), sr1);

    {
        const uint8_t bp_mask = W25Q32_SR1_BP0 | W25Q32_SR1_BP1 | W25Q32_SR1_BP2;
        w25q32_record_raw("SR1.BP=0", (uint8_t)((sr1 & bp_mask) == 0), sr1);
    }

    memset(g_w25q32_test_buf, 0x00, 4);
    W25q32_Read(g_w25q32, 0x000000, g_w25q32_test_buf, 4);
    w25q32_record("Read 4B@0", 1);

    W25q32_Sector_Erase(g_w25q32, 0x000000);
    w25q32_record("Sector Erase 4K", 1);

    memset(g_w25q32_test_buf, 0x00, W25Q32_TEST_BUF_SIZE);
    W25q32_Read(g_w25q32, 0x000000, g_w25q32_test_buf, W25Q32_TEST_BUF_SIZE);
    uint8_t all_ff = 1;
    for (uint32_t i = 0; i < W25Q32_TEST_BUF_SIZE; i++) {
        if (g_w25q32_test_buf[i] != 0xFF) {
            all_ff = 0;
            break;
        }
    }
    w25q32_record("Erased=0xFF x256", all_ff);

    for (uint32_t i = 0; i < W25Q32_TEST_BUF_SIZE; i++) { g_w25q32_test_buf[i] = (uint8_t)i; }
    W25q32_Page_Program(g_w25q32, 0x000000, g_w25q32_test_buf, W25Q32_TEST_BUF_SIZE);
    w25q32_record("Page Program 256B", 1);

    memset(g_w25q32_test_buf, 0x00, W25Q32_TEST_BUF_SIZE);
    W25q32_Read(g_w25q32, 0x000000, g_w25q32_test_buf, W25Q32_TEST_BUF_SIZE);
    uint8_t match = 1;
    for (uint32_t i = 0; i < W25Q32_TEST_BUF_SIZE; i++) {
        if (g_w25q32_test_buf[i] != (uint8_t)i) {
            match = 0;
            break;
        }
    }
    w25q32_record("Readback match 256B", match);

    W25q32_Sector_Erase(g_w25q32, 0x000000);
    memset(g_w25q32_test_buf, 0x00, W25Q32_TEST_BUF_SIZE);
    W25q32_Read(g_w25q32, 0x000000, g_w25q32_test_buf, W25Q32_TEST_BUF_SIZE);
    all_ff = 1;
    for (uint32_t i = 0; i < W25Q32_TEST_BUF_SIZE; i++) {
        if (g_w25q32_test_buf[i] != 0xFF) {
            all_ff = 0;
            break;
        }
    }
    w25q32_record("Re-erase=0xFF", all_ff);

    g_w25q32_all_passed = 1;
    for (uint8_t i = 0; i < g_w25q32_result_count; i++) {
        if (!g_w25q32_results[i].passed) {
            g_w25q32_all_passed = 0;
            break;
        }
    }
}

const W25q32_test_result* App_W25q32_Test_Get_Results(void) { return g_w25q32_results; }
uint8_t App_W25q32_Test_Get_Result_Count(void) { return g_w25q32_result_count; }
uint8_t App_W25q32_Test_All_Passed(void) { return g_w25q32_all_passed; }

static void w25q32_test_task(void* arg) {
    (void)arg;
    w25q32_init();
    while (1) {
        w25q32_loop();
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void Test_W25q32_Task_Def(void) { xTaskCreate(w25q32_test_task, "W25Q32_Test", 128, NULL, 1, NULL); }
