#include "score_store.h"

#include <stddef.h>
#include <string.h>

#include "board_config.h"

#if FRAMEWORK_USE_LFS
    #include "lfs.h"
    #include "lfs_port.h"
    #include "w25q32.h"
#endif

#include "rtt_log.h"

#define SCORE_STORE_MAGIC       0x53434f52u
#define SCORE_STORE_VERSION     1u
#define SCORE_STORE_MAX_GAMES   8u
#define SCORE_STORE_PATH        "/scores.bin"
#define SCORE_STORE_FLASH_START (2u * 1024u * 1024u)
#define SCORE_STORE_FLASH_SIZE  (2u * 1024u * 1024u)

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t game_count;
    uint32_t scores[SCORE_STORE_MAX_GAMES];
    uint32_t checksum;
} Score_file;

static Score_file g_scores;
static uint8_t g_available = 0;
static uint8_t g_dirty = 0;

#if FRAMEWORK_USE_LFS
static Lfs_port* g_port = NULL;

static uint32_t calculate_checksum(const Score_file* file) {
    const uint32_t* words = (const uint32_t*)file;
    uint32_t checksum = 0x91e10da5u;
    for (uint32_t i = 0; i < (sizeof(Score_file) / sizeof(uint32_t)) - 1u; i++) {
        checksum = (checksum << 5) | (checksum >> 27);
        checksum ^= words[i];
    }
    return checksum;
}

static uint8_t load_scores(void) {
    lfs_t* lfs = Lfs_Port_Get_Lfs(g_port);
    lfs_file_t file;
    if (lfs_file_open(lfs, &file, SCORE_STORE_PATH, LFS_O_RDONLY) != 0) { return 0; }

    Score_file loaded;
    const lfs_ssize_t read_size = lfs_file_read(lfs, &file, &loaded, sizeof(loaded));
    lfs_file_close(lfs, &file);

    if (read_size != sizeof(loaded) || loaded.magic != SCORE_STORE_MAGIC ||
        loaded.version != SCORE_STORE_VERSION || loaded.game_count > SCORE_STORE_MAX_GAMES ||
        loaded.checksum != calculate_checksum(&loaded)) {
        return 0;
    }

    g_scores = loaded;
    return 1;
}

static uint8_t save_scores(void) {
    lfs_t* lfs = Lfs_Port_Get_Lfs(g_port);
    lfs_file_t file;
    g_scores.checksum = calculate_checksum(&g_scores);

    if (lfs_file_open(lfs, &file, SCORE_STORE_PATH, LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC) != 0) {
        return 0;
    }
    const lfs_ssize_t written = lfs_file_write(lfs, &file, &g_scores, sizeof(g_scores));
    const int sync_result = lfs_file_sync(lfs, &file);
    lfs_file_close(lfs, &file);
    return written == sizeof(g_scores) && sync_result == 0;
}
#endif

void Score_Store_Init(uint8_t game_count) {
    const uint8_t desired_game_count =
        game_count > SCORE_STORE_MAX_GAMES ? SCORE_STORE_MAX_GAMES : game_count;
    memset(&g_scores, 0, sizeof(g_scores));
    g_scores.magic = SCORE_STORE_MAGIC;
    g_scores.version = SCORE_STORE_VERSION;
    g_scores.game_count = desired_game_count;

#if FRAMEWORK_USE_LFS
    const W25q32_config flash_config = {
        .spi_idx = SPI_LCD_IDX,
        .cs_gpio_idx = GPIO_SPI_CS_IDX,
    };
    W25q32* flash = W25q32_Create(&flash_config);
    if (flash == NULL || !W25q32_Init(flash)) {
        printf("[SAVE] W25Q32 unavailable, high scores are RAM-only\n");
        return;
    }

    const Lfs_port_config port_config = {
        .flash = flash,
        .start = SCORE_STORE_FLASH_START,
        .size = SCORE_STORE_FLASH_SIZE,
        .spi_mutex = NULL,
    };
    g_port = Lfs_Port_Create(&port_config);
    if (g_port == NULL) {
        printf("[SAVE] LittleFS port unavailable\n");
        return;
    }

    int result = Lfs_Port_Mount(g_port);
    if (result != 0) {
        result = Lfs_Port_Format(g_port);
        if (result == 0) { result = Lfs_Port_Mount(g_port); }
    }
    if (result != 0) {
        printf("[SAVE] LittleFS mount failed: %d\n", result);
        return;
    }

    g_available = 1;
    if (!load_scores()) {
        g_dirty = 1;
        Score_Store_Commit();
    } else if (g_scores.game_count != desired_game_count) {
        g_scores.game_count = desired_game_count;
        g_dirty = 1;
        Score_Store_Commit();
    }
    printf("[SAVE] high-score storage ready\n");
#else
    (void)game_count;
#endif
}

uint8_t Score_Store_Is_Available(void) { return g_available; }

uint32_t Score_Store_Get(uint8_t game_index) {
    return game_index < g_scores.game_count ? g_scores.scores[game_index] : 0;
}

void Score_Store_Observe(uint8_t game_index, uint32_t score) {
    if (game_index >= g_scores.game_count || score <= g_scores.scores[game_index]) { return; }
    g_scores.scores[game_index] = score;
    g_dirty = 1;
}

void Score_Store_Commit(void) {
    if (!g_dirty) { return; }
#if FRAMEWORK_USE_LFS
    if (g_available && save_scores()) {
        g_dirty = 0;
        printf("[SAVE] high scores committed\n");
    }
#endif
}
