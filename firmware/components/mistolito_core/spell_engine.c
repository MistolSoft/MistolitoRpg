#include "spell_engine.h"
#include "game_tables_structs.h"
#include "storage_task.h"
#include "spi_bus.h"
#include "esp_log.h"
#include "esp_random.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "SPELLS";

static uint8_t roll_d20(void)
{
    return (esp_random() % 20) + 1;
}

void spells_init(void)
{
}

static bool spell_is_known(pet_t *pet, const char *spell_id)
{
    for (uint8_t i = 0; i < pet->spells_known_count; i++) {
        if (strcmp(pet->spells_known[i].id, spell_id) == 0) {
            return true;
        }
    }
    return false;
}

static uint8_t get_max_spell_level(pet_t *pet)
{
    for (int i = 8; i >= 0; i--) {
        if (pet->spell_slots_max[i] > 0) {
            return (uint8_t)(i + 1);
        }
    }
    return 0;
}

uint8_t spells_get_available(pet_t *pet, spell_candidate_t *candidates, uint8_t max_count)
{
    if (pet == NULL || candidates == NULL) {
        return 0;
    }

    if (pet->profession != PROF_MAGE) {
        return 0;
    }

    uint8_t count = 0;
    uint8_t max_level = get_max_spell_level(pet);

    spi_bus_lock();

    FILE *f = fopen("/sdcard/DATA/TABLES/spells.bin", "rb");
    if (!f) {
        ESP_LOGW(TAG, "spells.bin not found");
        spi_bus_unlock();
        return 0;
    }

    spell_record_t rec;
    while (count < max_count && fread(&rec, sizeof(spell_record_t), 1, f) == 1) {
        if (rec.level <= max_level) {
            if (!spell_is_known(pet, rec.id)) {
                if (pet->dp >= (uint32_t)rec.dp_cost) {
                    strncpy(candidates[count].id, rec.id, sizeof(candidates[count].id) - 1);
                    candidates[count].id[sizeof(candidates[count].id) - 1] = '\0';

                    strncpy(candidates[count].name, rec.name, sizeof(candidates[count].name) - 1);
                    candidates[count].name[sizeof(candidates[count].name) - 1] = '\0';

                    candidates[count].level = rec.level;
                    candidates[count].dp_cost = rec.dp_cost;
                    candidates[count].success_dc = rec.success_dc;
                    count++;
                }
            }
        }
    }
    fclose(f);

    spi_bus_unlock();

    return count;
}

bool spells_try_learn(pet_t *pet, spell_candidate_t *candidate)
{
    if (pet == NULL || candidate == NULL) {
        return false;
    }

    if (pet->spells_known_count >= MAX_SPELLS_KNOWN) {
        ESP_LOGI(TAG, "Cannot learn more spells (max %d)", MAX_SPELLS_KNOWN);
        return false;
    }

    if (pet->dp < candidate->dp_cost) {
        ESP_LOGI(TAG, "Not enough DP for %s: have %lu, need %d",
                 candidate->name, pet->dp, candidate->dp_cost);
        return false;
    }

    uint8_t roll = roll_d20();
    bool success = (roll >= candidate->success_dc);

    pet->dp -= candidate->dp_cost;

    if (success) {
        snprintf(pet->spells_known[pet->spells_known_count].id, sizeof(pet->spells_known[0].id), "%s", candidate->id);
        pet->spells_known[pet->spells_known_count].level = candidate->level;
        pet->spells_known_count++;

        ESP_LOGI(TAG, "Spell %s learned! (roll=%d, DC=%d)",
                 candidate->name, roll, candidate->success_dc);
        return true;
    } else {
        ESP_LOGI(TAG, "Failed to learn %s (roll=%d, DC=%d), DP consumed",
                 candidate->name, roll, candidate->success_dc);
        return false;
    }
}
