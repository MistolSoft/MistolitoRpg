#include "skills_perks_engine.h"
#include "game_tables_structs.h"
#include "storage_task.h"
#include "spi_bus.h"
#include "esp_log.h"
#include "esp_random.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "SKILLS";

__attribute__((weak)) void on_skill_learned(pet_t *pet, uint8_t skill_slot) {}

static uint8_t roll_d20(void)
{
    return (esp_random() % 20) + 1;
}

bool skill_is_known(pet_t *pet, uint8_t skill_id)
{
    if (pet == NULL) return false;
    for (uint8_t i = 0; i < pet->skill_count; i++) {
        if (pet->skills[i].skill_id == skill_id) {
            return true;
        }
    }
    return false;
}

bool perk_is_known(pet_t *pet, uint8_t perk_id)
{
    if (pet == NULL) return false;
    for (uint8_t i = 0; i < pet->perk_count; i++) {
        if (pet->perks[i].id == perk_id) {
            return true;
        }
    }
    return false;
}

static bool check_stat_req_bin(pet_t *pet, const stat_req_t *req)
{
    if (pet->str < req->str) return false;
    if (pet->dex < req->dex) return false;
    if (pet->con < req->con) return false;
    if (pet->intel < req->intel) return false;
    if (pet->wis < req->wis) return false;
    if (pet->cha < req->cha) return false;
    return true;
}

static bool check_profession_req_bin(pet_t *pet, uint8_t profession_req_mask)
{
    return (profession_req_mask & (1 << pet->profession)) != 0;
}

bool skill_check_requirements(pet_t *pet, uint8_t skill_id, uint8_t profession_level)
{
    if (pet == NULL) return false;
    if (skill_is_known(pet, skill_id)) return false;

    spi_bus_lock();

    FILE *f = fopen("/sdcard/DATA/TABLES/skills.bin", "rb");
    if (!f) { spi_bus_unlock(); return false; }

    skill_record_t rec;
    bool meets = false;
    while (fread(&rec, sizeof(skill_record_t), 1, f) == 1) {
        if (rec.id == skill_id) {
            if (profession_level >= rec.profession_level_req &&
                check_profession_req_bin(pet, rec.profession_req_mask) &&
                check_stat_req_bin(pet, &rec.stat_req)) {
                meets = true;
            }
            break;
        }
    }
    fclose(f);

    spi_bus_unlock();

    return meets;
}

bool perk_check_requirements(pet_t *pet, uint8_t perk_id, uint8_t profession_level)
{
    if (pet == NULL) return false;
    if (perk_is_known(pet, perk_id)) return false;

    spi_bus_lock();

    FILE *f = fopen("/sdcard/DATA/TABLES/perks.bin", "rb");
    if (!f) { spi_bus_unlock(); return false; }

    perk_record_t rec;
    bool meets = false;
    while (fread(&rec, sizeof(perk_record_t), 1, f) == 1) {
        if (rec.id == perk_id) {
            if (profession_level >= rec.profession_level_req &&
                check_profession_req_bin(pet, rec.profession_req_mask) &&
                check_stat_req_bin(pet, &rec.stat_req)) {
                meets = true;
            }
            break;
        }
    }
    fclose(f);

    spi_bus_unlock();

    return meets;
}

void skills_perks_get_available(pet_t *pet, uint8_t profession_level, learn_candidate_t *candidates, uint8_t *count)
{
    if (pet == NULL || candidates == NULL || count == NULL) {
        if (count) *count = 0;
        return;
    }

    *count = 0;

    spi_bus_lock();

    FILE *f_skills = fopen("/sdcard/DATA/TABLES/skills.bin", "rb");
    if (f_skills) {
        skill_record_t rec;
        while (fread(&rec, sizeof(skill_record_t), 1, f_skills) == 1) {
            if (skill_check_requirements(pet, rec.id, profession_level)) {
                if (pet->dp >= (uint32_t)rec.dp_cost) {
                    candidates[*count].type = LEARN_TYPE_SKILL;
                    candidates[*count].id = rec.id;
                    candidates[*count].intent_type = rec.intent_type;
                    candidates[*count].dp_cost = rec.dp_cost;
                    candidates[*count].success_dc = rec.success_dc;
                    (*count)++;
                    if (*count >= 32) break;
                }
            }
        }
        fclose(f_skills);
    }

    if (*count < 32) {
        FILE *f_perks = fopen("/sdcard/DATA/TABLES/perks.bin", "rb");
        if (f_perks) {
            perk_record_t rec;
            while (fread(&rec, sizeof(perk_record_t), 1, f_perks) == 1) {
                if (perk_check_requirements(pet, rec.id, profession_level)) {
                    if (pet->dp >= (uint32_t)rec.dp_cost) {
                        candidates[*count].type = LEARN_TYPE_PERK;
                        candidates[*count].id = rec.id;
                        candidates[*count].intent_type = 0;
                        candidates[*count].dp_cost = rec.dp_cost;
                        candidates[*count].success_dc = rec.success_dc;
                        (*count)++;
                        if (*count >= 32) break;
                    }
                }
            }
            fclose(f_perks);
        }
    }

    spi_bus_unlock();
}

static void shuffle_candidates(learn_candidate_t *arr, uint8_t n)
{
    if (n <= 1) return;
    for (uint8_t i = n - 1; i > 0; i--) {
        uint8_t j = esp_random() % (i + 1);
        learn_candidate_t temp = arr[i];
        arr[i] = arr[j];
        arr[j] = temp;
    }
}

bool skills_perks_try_learn(pet_t *pet, learn_candidate_t *candidate)
{
    if (pet == NULL || candidate == NULL) return false;

    if (pet->dp < candidate->dp_cost) {
        ESP_LOGI(TAG, "Not enough DP: have %lu, need %d", pet->dp, candidate->dp_cost);
        return false;
    }

    uint8_t roll = roll_d20();
    bool success = (roll >= candidate->success_dc);

    pet->dp -= candidate->dp_cost;

    if (success) {
        if (candidate->type == LEARN_TYPE_SKILL) {
            if (pet->skill_count < MAX_SKILLS) {
                pet->skills[pet->skill_count].skill_id = candidate->id;
                pet->skills[pet->skill_count].intent_type = candidate->intent_type;
                pet->skills[pet->skill_count].uses_remaining = 3;
                pet->skills[pet->skill_count].uses_max = 3;
                uint8_t new_slot = pet->skill_count;
                pet->skill_count++;
                ESP_LOGI(TAG, "Skill %d learned! (slot=%d, intent=%d, roll=%d, DC=%d)", candidate->id, new_slot, candidate->intent_type, roll, candidate->success_dc);
                on_skill_learned(pet, new_slot);
            }
        } else {
            if (pet->perk_count < MAX_PERKS) {
                pet->perks[pet->perk_count].id = candidate->id;
                pet->perk_count++;
                ESP_LOGI(TAG, "Perk %d learned! (roll=%d, DC=%d)", candidate->id, roll, candidate->success_dc);
            }
        }
        return true;
    } else {
        ESP_LOGI(TAG, "Failed to learn %s %d (roll=%d, DC=%d), DP consumed",
            candidate->type == LEARN_TYPE_SKILL ? "skill" : "perk",
            candidate->id, roll, candidate->success_dc);
        return false;
    }
}

void skills_perks_process_level_up(pet_t *pet, uint8_t profession_level)
{
    if (pet == NULL) return;

    uint8_t open_slots = dna_get_open_skill_slots(&pet->dna, pet->level);
    if (pet->skill_count >= open_slots) {
        ESP_LOGI(TAG, "No open skill slots available (slots: %d, current: %d)", open_slots, pet->skill_count);
        return;
    }

    learn_candidate_t candidates[32];
    uint8_t count = 0;

    skills_perks_get_available(pet, profession_level, candidates, &count);

    if (count == 0) {
        ESP_LOGI(TAG, "No skills/perks available to learn");
        return;
    }

    shuffle_candidates(candidates, count);

    uint8_t to_learn = (count > MAX_LEARN_PER_LEVEL) ? MAX_LEARN_PER_LEVEL : count;
    uint8_t learned = 0;

    for (uint8_t i = 0; i < to_learn && i < count; i++) {
        if (skills_perks_try_learn(pet, &candidates[i])) {
            learned++;
        }
    }

    ESP_LOGI(TAG, "Level up processed: %d skills/perks learned", learned);
}
