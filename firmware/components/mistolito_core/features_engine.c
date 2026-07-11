#include "features_engine.h"
#include "game_tables_structs.h"
#include "storage_task.h"
#include "spi_bus.h"
#include "esp_log.h"
#include "esp_random.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "FEATURES";

static uint8_t roll_d20(void)
{
    return (esp_random() % 20) + 1;
}

void features_init(void)
{
}

static bool get_perk_name(uint8_t id, char *out_name, size_t max_len)
{
    FILE *f = fopen("/sdcard/DATA/TABLES/perks.bin", "rb");
    if (!f) return false;

    perk_record_t rec;
    bool found = false;
    while (fread(&rec, sizeof(perk_record_t), 1, f) == 1) {
        if (rec.id == id) {
            snprintf(out_name, max_len, "%s", rec.name);
            found = true;
            break;
        }
    }
    fclose(f);
    return found;
}

static bool get_skill_name(uint8_t id, char *out_name, size_t max_len)
{
    FILE *f = fopen("/sdcard/DATA/TABLES/skills.bin", "rb");
    if (!f) return false;

    skill_record_t rec;
    bool found = false;
    while (fread(&rec, sizeof(skill_record_t), 1, f) == 1) {
        if (rec.id == id) {
            snprintf(out_name, max_len, "%s", rec.name);
            found = true;
            break;
        }
    }
    fclose(f);
    return found;
}

static bool feature_is_known(pet_t *pet, const char *name)
{
    char tmp_name[32];
    for (uint8_t i = 0; i < pet->perk_count; i++) {
        if (get_perk_name(pet->perks[i].id, tmp_name, sizeof(tmp_name))) {
            if (strcmp(tmp_name, name) == 0) return true;
        }
    }

    for (uint8_t i = 0; i < pet->skill_count; i++) {
        if (get_skill_name(pet->skills[i].skill_id, tmp_name, sizeof(tmp_name))) {
            if (strcmp(tmp_name, name) == 0) return true;
        }
    }

    return false;
}

uint8_t features_get_available(pet_t *pet, uint8_t profession_level, feature_candidate_t *candidates, uint8_t max_count)
{
    if (pet == NULL || candidates == NULL || pet->profession == PROF_NONE) {
        return 0;
    }

    spi_bus_lock();

    FILE *f = fopen("/sdcard/DATA/TABLES/features.bin", "rb");
    if (!f) {
        ESP_LOGW(TAG, "features.bin not found");
        spi_bus_unlock();
        return 0;
    }

    uint8_t count = 0;
    feature_record_t rec;
    while (count < max_count && fread(&rec, sizeof(feature_record_t), 1, f) == 1) {
        if (rec.profession == pet->profession && rec.level == profession_level) {
            if (!feature_is_known(pet, rec.name)) {
                if (pet->dp >= rec.dp_cost) {
                    strncpy(candidates[count].name, rec.name, sizeof(candidates[count].name) - 1);
                    candidates[count].name[sizeof(candidates[count].name) - 1] = '\0';

                    candidates[count].dp_cost = rec.dp_cost;
                    candidates[count].success_dc = rec.success_dc;
                    strncpy(candidates[count].archetype_req, rec.archetype_req, sizeof(candidates[count].archetype_req) - 1);
                    candidates[count].archetype_req[sizeof(candidates[count].archetype_req) - 1] = '\0';
                    count++;
                }
            }
        }
    }
    fclose(f);

    spi_bus_unlock();

    return count;
}

bool features_try_learn(pet_t *pet, feature_candidate_t *candidate)
{
    if (pet == NULL || candidate == NULL) {
        return false;
    }

    if (pet->dp < candidate->dp_cost) {
        ESP_LOGI(TAG, "Not enough DP for %s: have %lu, need %d", 
                 candidate->name, pet->dp, candidate->dp_cost);
        return false;
    }

    spi_bus_lock();

    uint8_t roll = roll_d20();
    bool success = (roll >= candidate->success_dc);

    pet->dp -= candidate->dp_cost;

    if (success) {
        FILE *f_skills = fopen("/sdcard/DATA/TABLES/skills.bin", "rb");
        bool learned = false;
        if (f_skills) {
            skill_record_t rec;
            while (fread(&rec, sizeof(skill_record_t), 1, f_skills) == 1) {
                if (strcmp(rec.name, candidate->name) == 0) {
                    if (pet->skill_count < MAX_SKILLS) {
                        pet->skills[pet->skill_count].skill_id = rec.id;
                        pet->skills[pet->skill_count].intent_type = rec.intent_type;
                        pet->skills[pet->skill_count].uses_remaining = 3;
                        pet->skills[pet->skill_count].uses_max = 3;
                        pet->skill_count++;
                        ESP_LOGI(TAG, "Feature %s learned as skill! (roll=%d, DC=%d)", 
                                 candidate->name, roll, candidate->success_dc);
                        learned = true;
                    }
                    break;
                }
            }
            fclose(f_skills);
        }

        if (!learned) {
            FILE *f_perks = fopen("/sdcard/DATA/TABLES/perks.bin", "rb");
            if (f_perks) {
                perk_record_t rec;
                while (fread(&rec, sizeof(perk_record_t), 1, f_perks) == 1) {
                    if (strcmp(rec.name, candidate->name) == 0) {
                        if (pet->perk_count < MAX_PERKS) {
                            pet->perks[pet->perk_count].id = rec.id;
                            pet->perk_count++;
                            ESP_LOGI(TAG, "Feature %s learned as perk! (roll=%d, DC=%d)", 
                                     candidate->name, roll, candidate->success_dc);
                            learned = true;
                        }
                        break;
                    }
                }
                fclose(f_perks);
            }
        }

        spi_bus_unlock();
        return learned;
    } else {
        ESP_LOGI(TAG, "Failed to learn %s (roll=%d, DC=%d), DP consumed", 
                 candidate->name, roll, candidate->success_dc);
        spi_bus_unlock();
        return false;
    }
}
