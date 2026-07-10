#include <assert.h>
#include <stdint.h>

#include "score_store.h"

int main(void) {
    static const uint8_t expected[] = {
        0u, 1u, 0xffu, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u,
        10u, 11u, 12u, 16u, 17u, 20u, 15u, 18u, 13u, 14u, 19u,
    };
    for (uint8_t old_id = 0; old_id < sizeof(expected); old_id++) {
        assert(Score_Store_Map_Legacy_Id(old_id) == expected[old_id]);
    }
    assert(Score_Store_Map_Legacy_Id((uint8_t)sizeof(expected)) == 0xffu);
    assert(Score_Store_Map_Legacy_Id(0xffu) == 0xffu);

    uint8_t old_counts[SCORE_STORE_MAX_GAMES] = {0};
    Score_entry old_entries[SCORE_STORE_MAX_GAMES][SCORE_STORE_TOP_COUNT] = {0};
    uint8_t new_counts[SCORE_STORE_MAX_GAMES] = {0};
    Score_entry new_entries[SCORE_STORE_MAX_GAMES][SCORE_STORE_TOP_COUNT] = {0};
    for (uint8_t old_id = 0; old_id < sizeof(expected); old_id++) {
        old_counts[old_id] = 1u;
        old_entries[old_id][0].score = 1000u + old_id;
        old_entries[old_id][0].name[0] = (char)('A' + old_id);
    }
    Score_Store_Migrate_Legacy_Entries(
        (uint8_t)sizeof(expected), old_counts, old_entries, 21u, new_counts, new_entries);
    for (uint8_t old_id = 0; old_id < sizeof(expected); old_id++) {
        const uint8_t new_id = expected[old_id];
        if (new_id == 0xffu) { continue; }
        assert(new_counts[new_id] == 1u);
        assert(new_entries[new_id][0].score == 1000u + old_id);
        assert(new_entries[new_id][0].name[0] == (char)('A' + old_id));
    }
    return 0;
}
