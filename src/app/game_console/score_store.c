#include "score_store.h"

#include <stddef.h>
#include <string.h>

#if FRAMEWORK_USE_LFS
    #include "lfs.h"
    #include "storage.h"
#endif

#include "rtt_log.h"

#define SCORE_STORE_MAGIC   0x53434f52u
#define SCORE_STORE_VERSION 4u
#define SCORE_STORE_PATH    "/scores.bin"

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t game_count;
    uint32_t scores[SCORE_STORE_MAX_GAMES];
    uint32_t checksum;
} Score_file_v1;

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t game_count;
    uint8_t entry_count[SCORE_STORE_MAX_GAMES];
    Score_entry entries[SCORE_STORE_MAX_GAMES][SCORE_STORE_TOP_COUNT];
    uint32_t checksum;
} Score_file;

static Score_file g_scores;
static uint8_t g_available = 0;
static uint8_t g_dirty = 0;

uint8_t Score_Store_Map_Legacy_Id(uint8_t legacy_id) {
    static const uint8_t legacy_to_current[] = {
        0u, 1u, 0xffu, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u, 10u, 11u, 12u, 16u, 17u, 20u, 15u, 18u, 13u, 14u, 19u};
    return legacy_id < sizeof(legacy_to_current) ? legacy_to_current[legacy_id] : 0xffu;
}

void Score_Store_Migrate_Legacy_Entries(uint8_t legacy_game_count,
    const uint8_t legacy_entry_count[SCORE_STORE_MAX_GAMES],
    const Score_entry legacy_entries[SCORE_STORE_MAX_GAMES][SCORE_STORE_TOP_COUNT],
    uint8_t current_game_count, uint8_t current_entry_count[SCORE_STORE_MAX_GAMES],
    Score_entry current_entries[SCORE_STORE_MAX_GAMES][SCORE_STORE_TOP_COUNT]) {
    memset(current_entry_count, 0, SCORE_STORE_MAX_GAMES * sizeof(current_entry_count[0]));
    memset(current_entries, 0, SCORE_STORE_MAX_GAMES * sizeof(current_entries[0]));
    if (legacy_game_count > SCORE_STORE_MAX_GAMES) { legacy_game_count = SCORE_STORE_MAX_GAMES; }
    if (current_game_count > SCORE_STORE_MAX_GAMES) { current_game_count = SCORE_STORE_MAX_GAMES; }
    for (uint8_t old_id = 0; old_id < legacy_game_count; old_id++) {
        const uint8_t new_id = Score_Store_Map_Legacy_Id(old_id);
        if (new_id == 0xffu || new_id >= current_game_count) { continue; }
        uint8_t count = legacy_entry_count[old_id];
        if (count > SCORE_STORE_TOP_COUNT) { count = SCORE_STORE_TOP_COUNT; }
        current_entry_count[new_id] = count;
        memcpy(current_entries[new_id], legacy_entries[old_id], count * sizeof(Score_entry));
    }
}

static void reset_scores(uint8_t game_count) {
    memset(&g_scores, 0, sizeof(g_scores));
    g_scores.magic = SCORE_STORE_MAGIC;
    g_scores.version = SCORE_STORE_VERSION;
    g_scores.game_count = game_count;
}

#if FRAMEWORK_USE_LFS
static uint32_t calculate_checksum_words(const void* data, uint32_t word_count) {
    const uint32_t* words = (const uint32_t*)data;
    uint32_t checksum = 0x91e10da5u;
    for (uint32_t i = 0; i < word_count; i++) {
        checksum = (checksum << 5) | (checksum >> 27);
        checksum ^= words[i];
    }
    return checksum;
}

static uint32_t calculate_checksum(const Score_file* file) {
    const uint32_t* words = (const uint32_t*)file;
    return calculate_checksum_words(words, (sizeof(Score_file) / sizeof(uint32_t)) - 1u);
}

static uint32_t calculate_v1_checksum(const Score_file_v1* file) {
    return calculate_checksum_words(file, (sizeof(Score_file_v1) / sizeof(uint32_t)) - 1u);
}

static void migrate_v1(const Score_file_v1* old_file, uint8_t game_count) {
    reset_scores(game_count);
    const uint8_t copy_count =
        old_file->game_count < SCORE_STORE_MAX_GAMES ? (uint8_t)old_file->game_count : SCORE_STORE_MAX_GAMES;
    for (uint8_t old_id = 0; old_id < copy_count; old_id++) {
        const uint8_t new_id = Score_Store_Map_Legacy_Id(old_id);
        if (new_id == 0xffu || new_id >= game_count || old_file->scores[old_id] == 0) { continue; }
        Score_entry* entry = &g_scores.entries[new_id][0];
        memcpy(entry->name, "LEGACY", SCORE_STORE_NAME_LENGTH);
        entry->name[SCORE_STORE_NAME_LENGTH] = '\0';
        entry->score = old_file->scores[old_id];
        g_scores.entry_count[new_id] = 1;
    }
    g_dirty = 1;
}

static uint8_t load_scores(uint8_t game_count) {
    lfs_t* lfs = Storage_Get_Lfs();
    if (lfs == NULL) { return 0; }

    Storage_Lock();
    lfs_file_t file;
    if (lfs_file_open(lfs, &file, SCORE_STORE_PATH, LFS_O_RDONLY) != 0) {
        Storage_Unlock();
        return 0;
    }

    const lfs_soff_t file_size = lfs_file_size(lfs, &file);
    lfs_ssize_t read_size = -1;
    uint8_t migrated = 0;
    if (file_size == (lfs_soff_t)sizeof(Score_file)) {
        Score_file loaded;
        read_size = lfs_file_read(lfs, &file, &loaded, sizeof(loaded));
        if (read_size == sizeof(loaded) && loaded.magic == SCORE_STORE_MAGIC &&
            loaded.game_count <= SCORE_STORE_MAX_GAMES && loaded.checksum == calculate_checksum(&loaded)) {
            if (loaded.version == SCORE_STORE_VERSION) {
                g_scores = loaded;
            } else if (loaded.version == 3u) {
                reset_scores(game_count);
                Score_Store_Migrate_Legacy_Entries(loaded.game_count, loaded.entry_count, loaded.entries,
                    game_count, g_scores.entry_count, g_scores.entries);
                g_dirty = 1;
                migrated = 1;
            }
        }
    } else if (file_size == (lfs_soff_t)sizeof(Score_file_v1)) {
        Score_file_v1 old_file;
        read_size = lfs_file_read(lfs, &file, &old_file, sizeof(old_file));
        if (read_size == sizeof(old_file) && old_file.magic == SCORE_STORE_MAGIC && old_file.version == 1u &&
            old_file.game_count <= SCORE_STORE_MAX_GAMES &&
            old_file.checksum == calculate_v1_checksum(&old_file)) {
            migrate_v1(&old_file, game_count);
            migrated = 1;
        }
    }
    lfs_file_close(lfs, &file);
    Storage_Unlock();
    if (migrated) { return 1; }

    if (read_size != sizeof(g_scores) || g_scores.magic != SCORE_STORE_MAGIC ||
        g_scores.version != SCORE_STORE_VERSION || g_scores.game_count > SCORE_STORE_MAX_GAMES ||
        g_scores.checksum != calculate_checksum(&g_scores)) {
        return 0;
    }

    for (int game = 0; game < g_scores.game_count; game++) {
        if (g_scores.entry_count[game] > SCORE_STORE_TOP_COUNT) { return 0; }
        for (uint8_t rank = 0; rank < g_scores.entry_count[game]; rank++) {
            g_scores.entries[game][rank].name[SCORE_STORE_NAME_LENGTH] = '\0';
        }
    }
    return 1;
}

static uint8_t save_scores(void) {
    lfs_t* lfs = Storage_Get_Lfs();
    if (lfs == NULL) { return 0; }

    lfs_file_t file;
    g_scores.checksum = calculate_checksum(&g_scores);

    Storage_Lock();
    if (lfs_file_open(lfs, &file, SCORE_STORE_PATH, LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC) != 0) {
        Storage_Unlock();
        return 0;
    }
    const lfs_ssize_t written = lfs_file_write(lfs, &file, &g_scores, sizeof(g_scores));
    const int sync_result = lfs_file_sync(lfs, &file);
    lfs_file_close(lfs, &file);
    Storage_Unlock();
    return written == sizeof(g_scores) && sync_result == 0;
}
#endif

void Score_Store_Init(uint8_t game_count) {
    const uint8_t desired_game_count =
        game_count > SCORE_STORE_MAX_GAMES ? SCORE_STORE_MAX_GAMES : game_count;
    reset_scores(desired_game_count);

#if FRAMEWORK_USE_LFS
    if (!Storage_Is_Available()) {
        printf("[SAVE] external storage unavailable, high scores are RAM-only\n");
        return;
    }

    g_available = 1;
    if (!load_scores(desired_game_count)) {
        reset_scores(desired_game_count);
        g_dirty = 1;
        Score_Store_Commit();
    } else {
        if (g_scores.game_count != desired_game_count) {
            g_scores.game_count = desired_game_count;
            g_dirty = 1;
        }
        Score_Store_Commit();
    }
    printf("[SAVE] high-score storage ready\n");
#else
    (void)game_count;
#endif
}

uint8_t Score_Store_Is_Available(void) { return g_available; }

uint32_t Score_Store_Get(uint8_t game_index) {
    const Score_entry* entry = Score_Store_Get_Entry(game_index, 0);
    return entry == NULL ? 0 : entry->score;
}

uint8_t Score_Store_Qualifies(uint8_t game_index, uint32_t score) {
    if (game_index >= g_scores.game_count) { return 0; }
    const uint8_t count = g_scores.entry_count[game_index];
    return count < SCORE_STORE_TOP_COUNT ||
           score > g_scores.entries[game_index][SCORE_STORE_TOP_COUNT - 1u].score;
}

uint8_t Score_Store_Add(uint8_t game_index, const char* name, uint32_t score) {
    if (game_index >= g_scores.game_count || name == NULL || !Score_Store_Qualifies(game_index, score)) {
        return 0xffu;
    }

    uint8_t count = g_scores.entry_count[game_index];
    uint8_t insert_at = count;
    for (uint8_t rank = 0; rank < count; rank++) {
        if (score > g_scores.entries[game_index][rank].score) {
            insert_at = rank;
            break;
        }
    }
    if (insert_at >= SCORE_STORE_TOP_COUNT) { return 0xffu; }

    const uint8_t last = count < SCORE_STORE_TOP_COUNT ? count : (uint8_t)(SCORE_STORE_TOP_COUNT - 1u);
    for (uint8_t rank = last; rank > insert_at; rank--) {
        g_scores.entries[game_index][rank] = g_scores.entries[game_index][rank - 1u];
    }

    Score_entry* entry = &g_scores.entries[game_index][insert_at];
    memset(entry, 0, sizeof(*entry));
    for (uint8_t i = 0; i < SCORE_STORE_NAME_LENGTH && name[i] != '\0'; i++) {
        const char character = name[i];
        entry->name[i] = ((character >= 'A' && character <= 'Z') || (character >= '0' && character <= '9'))
                             ? character
                             : '-';
    }
    entry->score = score;
    if (count < SCORE_STORE_TOP_COUNT) { g_scores.entry_count[game_index] = count + 1u; }
    g_dirty = 1;
    return insert_at;
}

uint8_t Score_Store_Get_Count(uint8_t game_index) {
    return game_index < g_scores.game_count ? g_scores.entry_count[game_index] : 0;
}

const Score_entry* Score_Store_Get_Entry(uint8_t game_index, uint8_t rank) {
    if (game_index >= g_scores.game_count || rank >= g_scores.entry_count[game_index]) { return NULL; }
    return &g_scores.entries[game_index][rank];
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
