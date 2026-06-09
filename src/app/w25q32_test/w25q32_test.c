#include "w25q32_test.h"

#include <string.h>

#include "board_config.h"
#include "bsp_gpio.h"
#include "w25q32.h"

// === Module-private state ===

static W25q32* g_w25q32 = NULL;
static volatile bool g_w25q32_jedec_ok = false;

#define W25Q32_TEST_BUF_SIZE 256
static uint8_t g_w25q32_test_buf[W25Q32_TEST_BUF_SIZE];

#define W25Q32_TEST_RESULT_MAX 8
static W25q32_test_result g_w25q32_results[W25Q32_TEST_RESULT_MAX];
static uint8_t g_w25q32_result_count = 0;
static bool g_w25q32_all_passed = false;

static void w25q32_record(const char* name, bool passed) {
    if (g_w25q32_result_count < W25Q32_TEST_RESULT_MAX) {
        g_w25q32_results[g_w25q32_result_count].name = name;
        g_w25q32_results[g_w25q32_result_count].passed = passed;
        g_w25q32_results[g_w25q32_result_count].raw = 0;
        g_w25q32_result_count++;
    }
}

static void w25q32_record_raw(const char* name, bool passed, uint8_t raw) {
    if (g_w25q32_result_count < W25Q32_TEST_RESULT_MAX) {
        g_w25q32_results[g_w25q32_result_count].name = name;
        g_w25q32_results[g_w25q32_result_count].passed = passed;
        g_w25q32_results[g_w25q32_result_count].raw = raw;
        g_w25q32_result_count++;
    }
}

// === Public API ===

void App_W25q32_Test_Init(void) {
    const W25q32_config cfg = {
        .spi_idx = SPI_LCD_IDX,
        .cs_gpio_idx = GPIO_SPI_CS_IDX,
    };
    g_w25q32 = W25q32_Create(&cfg);
    g_w25q32_jedec_ok = W25q32_Init(g_w25q32);

    // Bsp_Gpio_Write(GPIO_TFT_BLK_IDX, g_w25q32_jedec_ok ? bsp_gpio_state_set : bsp_gpio_state_reset);
}

// Comprehensive SPI flash test battery.
//
// Exercises the W25Q32 protocol paths in bsp_spi:
//   1. SR1.BUSY=0          (Bsp_Spi_Write 1B + Bsp_Spi_Read 1B)
//   2. Read 4B from addr 0 (Bsp_Spi_Write 4B + Bsp_Spi_Read 4B)
//   3. Sector erase 4K     (Bsp_Spi_Write 4B + polling SR1 BUSY)
//   4. Erased=0xFF x256    (Bsp_Spi_Read 256B)
//   5. Page Program 256B   (Bsp_Spi_Write 4B + 256B + polling BUSY)
//   6. Readback match 256B (Bsp_Spi_Read 256B)
//   7. Re-erase=0xFF       (full round-trip)
void App_W25q32_Test_Loop(void) {
    if (g_w25q32 == NULL) { return; }

    g_w25q32_result_count = 0;

    // Test 1: SR1.BUSY must be 0 (chip is idle). Record the raw SR1 so
    // we can see which bits are set when this fails.
    uint8_t sr1 = W25q32_Read_Status_Reg_1(g_w25q32);
    w25q32_record_raw("SR1.BUSY=0", (sr1 & W25Q32_SR1_BUSY) == 0, sr1);

    // Test 2: SR1 block-protect bits must be 0. If any of BP0/BP1/BP2 is
    // set, the corresponding top-of-array region is silently write/erase-
    // protected. Raw SR1 is recorded for visibility.
    {
        const uint8_t bp_mask = W25Q32_SR1_BP0 | W25Q32_SR1_BP1 | W25Q32_SR1_BP2;
        w25q32_record_raw("SR1.BP=0", (sr1 & bp_mask) == 0, sr1);
    }

    // Test 3: Read 4 bytes from address 0
    memset(g_w25q32_test_buf, 0x00, 4);
    W25q32_Read(g_w25q32, 0x000000, g_w25q32_test_buf, 4);
    w25q32_record("Read 4B@0", true);

    // Test 4: Sector erase
    W25q32_Sector_Erase(g_w25q32, 0x000000);
    w25q32_record("Sector Erase 4K", true);

    // Test 5: Verify erased
    memset(g_w25q32_test_buf, 0x00, W25Q32_TEST_BUF_SIZE);
    W25q32_Read(g_w25q32, 0x000000, g_w25q32_test_buf, W25Q32_TEST_BUF_SIZE);
    bool all_ff = true;
    for (uint32_t i = 0; i < W25Q32_TEST_BUF_SIZE; i++) {
        if (g_w25q32_test_buf[i] != 0xFF) {
            all_ff = false;
            break;
        }
    }
    w25q32_record("Erased=0xFF x256", all_ff);

    // Test 6: Page program incrementing pattern
    for (uint32_t i = 0; i < W25Q32_TEST_BUF_SIZE; i++) { g_w25q32_test_buf[i] = (uint8_t)i; }
    W25q32_Page_Program(g_w25q32, 0x000000, g_w25q32_test_buf, W25Q32_TEST_BUF_SIZE);
    w25q32_record("Page Program 256B", true);

    // Test 7: Read back + compare
    memset(g_w25q32_test_buf, 0x00, W25Q32_TEST_BUF_SIZE);
    W25q32_Read(g_w25q32, 0x000000, g_w25q32_test_buf, W25Q32_TEST_BUF_SIZE);
    bool match = true;
    for (uint32_t i = 0; i < W25Q32_TEST_BUF_SIZE; i++) {
        if (g_w25q32_test_buf[i] != (uint8_t)i) {
            match = false;
            break;
        }
    }
    w25q32_record("Readback match 256B", match);

    // Test 8: Re-erase + verify
    W25q32_Sector_Erase(g_w25q32, 0x000000);
    memset(g_w25q32_test_buf, 0x00, W25Q32_TEST_BUF_SIZE);
    W25q32_Read(g_w25q32, 0x000000, g_w25q32_test_buf, W25Q32_TEST_BUF_SIZE);
    all_ff = true;
    for (uint32_t i = 0; i < W25Q32_TEST_BUF_SIZE; i++) {
        if (g_w25q32_test_buf[i] != 0xFF) {
            all_ff = false;
            break;
        }
    }
    w25q32_record("Re-erase=0xFF", all_ff);

    // Aggregate
    g_w25q32_all_passed = true;
    for (uint8_t i = 0; i < g_w25q32_result_count; i++) {
        if (!g_w25q32_results[i].passed) {
            g_w25q32_all_passed = false;
            break;
        }
    }

    // Bsp_Gpio_Write(GPIO_TFT_BLK_IDX, g_w25q32_all_passed ? bsp_gpio_state_set : bsp_gpio_state_reset);
}

const W25q32_test_result* App_W25q32_Test_Get_Results(void) { return g_w25q32_results; }
uint8_t App_W25q32_Test_Get_Result_Count(void) { return g_w25q32_result_count; }
bool App_W25q32_Test_All_Passed(void) { return g_w25q32_all_passed; }
