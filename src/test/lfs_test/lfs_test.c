#include "lfs_test.h"

#include <stdint.h>
#include <string.h>

#include "FreeRTOS.h"
#include "board_config.h"
#include "bsp_gpio.h"
#include "lfs.h"
#include "lfs_port.h"
#include "rtt_log.h"
#include "task.h"
#include "w25q32.h"

#define LFS_TEST_FLASH_START (2u * 1024u * 1024u) /* 2 MiB */
#define LFS_TEST_FLASH_SIZE  (2u * 1024u * 1024u) /* 2 MiB */

#define LFS_TEST_PAYLOAD     "hello littlefs - tick=%u"
#define LFS_TEST_RESULT_MAX  16

#if FRAMEWORK_USE_LFS

typedef struct {
    const char* name;
    uint8_t passed;
    int lfs_err; /**< raw lfs error code (0 = ok) for diagnosis */
} Lfs_test_result;

static W25q32* g_flash = NULL;
static Lfs_port* g_port = NULL;

static Lfs_test_result g_results[LFS_TEST_RESULT_MAX];
static uint8_t g_result_count = 0;
static uint8_t g_all_passed = 0;

static void record(const char* name, int err) {
    if (g_result_count >= LFS_TEST_RESULT_MAX) { return; }
    g_results[g_result_count].name = name;
    g_results[g_result_count].lfs_err = err;
    g_results[g_result_count].passed = (uint8_t)(err == 0);
    g_result_count++;
    printf("  [%s] %-32s err=%d\n", err == 0 ? " OK " : "FAIL", name, err);
}

static void record_noerr(const char* name, uint8_t ok) { record(name, ok ? 0 : -1); }

void App_Lfs_Test_Init(void) {
    const W25q32_config cfg = {
        .spi_idx = SPI_LCD_IDX,
        .cs_gpio_idx = GPIO_SPI_CS_IDX,
    };
    g_flash = W25q32_Create(&cfg);
    if (!W25q32_Init(g_flash)) {
        record("W25Q32 init", -1);
        g_all_passed = 0;
        return;
    }
    record("W25Q32 init", 0);

    extern char __heap_start__[];  // NOLINT (readability-identifier-naming)
    extern char __HeapLimit[];     // NOLINT (readability-identifier-naming)
    printf("       newlib heap: %p..%p (%u B)\n", (void*)__heap_start__, (void*)__HeapLimit,
        (unsigned)((char*)__HeapLimit - (char*)__heap_start__));

    const Lfs_port_config lfs_cfg = {
        .flash = g_flash,
        .start = LFS_TEST_FLASH_START,
        .size = LFS_TEST_FLASH_SIZE,
    };
    g_port = Lfs_Port_Create(&lfs_cfg);
    if (g_port == NULL) {
        record("Lfs_Port_Create", -1);
        g_all_passed = 0;
        return;
    }
    record("Lfs_Port_Create", 0);

    int err;

    err = Lfs_Port_Format(g_port);
    record("format", err);
    if (err != 0) {
        g_all_passed = 0;
        return;
    }

    err = Lfs_Port_Mount(g_port);
    record("mount (after format)", err);
    if (err != 0) {
        g_all_passed = 0;
        return;
    }

    lfs_t* lfs = Lfs_Port_Get_Lfs(g_port);

    {
        lfs_file_t f;
        err = lfs_file_open(lfs, &f, "boot_count", LFS_O_WRONLY | LFS_O_CREAT);
        if (err == 0) {
            char buf[64];
            int n = snprintf(buf, sizeof(buf), LFS_TEST_PAYLOAD, 1u);
            lfs_ssize_t w = lfs_file_write(lfs, &f, buf, (lfs_size_t)n);
            err = (w == (lfs_ssize_t)n) ? 0 : (int)w;
            lfs_file_close(lfs, &f);
        }
        record("write boot_count", err);
    }

    {
        lfs_file_t f;
        err = lfs_file_open(lfs, &f, "boot_count", LFS_O_RDONLY);
        uint8_t match = 0;
        if (err == 0) {
            char read_buf[64] = {0};
            lfs_size_t n = lfs_file_read(lfs, &f, read_buf, sizeof(read_buf) - 1);
            lfs_file_close(lfs, &f);

            char expected[64];
            int en = snprintf(expected, sizeof(expected), LFS_TEST_PAYLOAD, 1u);
            match = (n == (lfs_size_t)en) && (memcmp(read_buf, expected, en) == 0);
        }
        record_noerr("read+verify boot_count", err == 0 && match);
        if (err != 0) { record("  (read)", err); }
    }

    {
        struct lfs_info info;
        err = lfs_stat(lfs, "boot_count", &info);
        uint8_t ok = (uint8_t)((err == 0) && (info.size > 0) && (info.type == LFS_TYPE_REG));
        record_noerr("stat boot_count (size>0)", ok);
        if (err == 0) { printf("       size=%lu type=%d\n", (unsigned long)info.size, (int)info.type); }
    }

    {
        lfs_file_t f;
        err = lfs_file_open(lfs, &f, "config.txt", LFS_O_WRONLY | LFS_O_CREAT);
        if (err == 0) {
            const char* body = "mode=test\nbuild=" __DATE__ "\n";
            lfs_size_t want = (lfs_size_t)strlen(body);
            lfs_ssize_t w = lfs_file_write(lfs, &f, body, want);
            err = (w == (lfs_ssize_t)want) ? 0 : (int)w;
            lfs_file_close(lfs, &f);
        }
        record("write config.txt", err);
    }

    {
        lfs_dir_t dir;
        err = lfs_dir_open(lfs, &dir, "/");
        int count = 0;
        if (err == 0) {
            struct lfs_info info;
            while (lfs_dir_read(lfs, &dir, &info) > 0) {
                /* Skip the root's "." and ".." self-references littlefs
                 * emits on every dir_read; they're not user files. */
                if (info.name[0] == '.' &&
                    (info.name[1] == '\0' || (info.name[1] == '.' && info.name[2] == '\0'))) {
                    continue;
                }
                printf(
                    "       /%-16s size=%lu type=%d\n", info.name, (unsigned long)info.size, (int)info.type);
                count++;
            }
            lfs_dir_close(lfs, &dir);
        }
        record_noerr("dir list (2 files expected)", err == 0 && count == 2);
    }

    err = Lfs_Port_Unmount(g_port);
    record("unmount", err);

    err = Lfs_Port_Mount(g_port);
    record("remount", err);
    if (err == 0) {
        lfs_t* lfs2 = Lfs_Port_Get_Lfs(g_port);
        struct lfs_info info;
        uint8_t boot_ok = (uint8_t)((lfs_stat(lfs2, "boot_count", &info) == 0) && (info.size > 0));
        uint8_t cfg_ok = (uint8_t)((lfs_stat(lfs2, "config.txt", &info) == 0) && (info.size > 0));
        record_noerr("persisted boot_count", boot_ok);
        record_noerr("persisted config.txt", cfg_ok);
    }

    {
        lfs_t* lfs3 = Lfs_Port_Get_Lfs(g_port);
        err = lfs_remove(lfs3, "boot_count");
        record("unlink boot_count", err);

        lfs_dir_t dir;
        int count = 0;
        if (lfs_dir_open(lfs3, &dir, "/") == 0) {
            struct lfs_info info;
            while (lfs_dir_read(lfs3, &dir, &info) > 0) {
                /* Same "." / ".." skip as the listing above. */
                if (info.name[0] == '.' &&
                    (info.name[1] == '\0' || (info.name[1] == '.' && info.name[2] == '\0'))) {
                    continue;
                }
                count++;
            }
            lfs_dir_close(lfs3, &dir);
        }
        record_noerr("dir list after unlink (1 expected)", count == 1);
    }

    g_all_passed = 1;
    for (uint8_t i = 0; i < g_result_count; i++) {
        if (!g_results[i].passed) {
            g_all_passed = 0;
            break;
        }
    }

    Bsp_Gpio_Write(GPIO_TFT_BLK_IDX, g_all_passed ? bsp_gpio_state_set : bsp_gpio_state_reset);

    printf("\n========== lfs_test results (paste this) ==========\n");
    uint8_t passed = 0;
    for (uint8_t i = 0; i < g_result_count; i++) {
        if (g_results[i].passed) { passed++; }
        printf("  [%2u] %-32s err=%-4d %s\n", (unsigned)i, g_results[i].name, g_results[i].lfs_err,
            g_results[i].passed ? "PASS" : "FAIL");
    }
    printf("  ---- %u/%u passed ----\n", (unsigned)passed, (unsigned)g_result_count);
    printf("====================================================\n");

    printf("lfs_test: %s (%u/%u passed)\n", g_all_passed ? "PASS" : "FAIL", (unsigned)passed,
        (unsigned)g_result_count);
}

void App_Lfs_Test_Loop(void) {}

#else

void App_Lfs_Test_Init(void) {}
void App_Lfs_Test_Loop(void) {}

#endif

static void lfs_test_task(void* arg) {
    (void)arg;
    App_Lfs_Test_Init();
    while (1) {
        App_Lfs_Test_Loop();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void Lfs_Test_Task_Def(void) { xTaskCreate(lfs_test_task, "LFS_Test", 1024, NULL, 1, NULL); }
