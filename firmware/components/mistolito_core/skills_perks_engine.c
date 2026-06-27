#include "skills_perks_engine.h"
#include "storage_task.h"
#include "esp_log.h"
#include "esp_random.h"
#include "cJSON.h"
#include <string.h>

static const char *TAG = "SKILLS";

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

static bool check_stat_req(pet_t *pet, cJSON *req)
{
    if (!req) return true;

    cJSON *str_req = cJSON_GetObjectItem(req, "str");
    if (str_req && pet->str < (uint8_t)str_req->valueint) return false;

    cJSON *dex_req = cJSON_GetObjectItem(req, "dex");
    if (dex_req && pet->dex < (uint8_t)dex_req->valueint) return false;

    cJSON *con_req = cJSON_GetObjectItem(req, "con");
    if (con_req && pet->con < (uint8_t)con_req->valueint) return false;

    cJSON *int_req = cJSON_GetObjectItem(req, "int");
    if (int_req && pet->intel < (uint8_t)int_req->valueint) return false;

    cJSON *wis_req = cJSON_GetObjectItem(req, "wis");
    if (wis_req && pet->wis < (uint8_t)wis_req->valueint) return false;

    cJSON *cha_req = cJSON_GetObjectItem(req, "cha");
    if (cha_req && pet->cha < (uint8_t)cha_req->valueint) return false;

    return true;
}

static bool check_profession_req(pet_t *pet, cJSON *prof_req)
{
    if (!prof_req || !cJSON_IsArray(prof_req)) return false;

    cJSON *prof_item = NULL;
    cJSON_ArrayForEach(prof_item, prof_req) {
        if (prof_item->valueint == (int)pet->profession) {
            return true;
        }
    }
    return false;
}

bool skill_check_requirements(pet_t *pet, uint8_t skill_id, uint8_t profession_level)
{
    if (pet == NULL) return false;
    if (skill_is_known(pet, skill_id)) return false;

    const char *json = storage_get_game_tables_json();
    if (!json) return false;

    cJSON *root = cJSON_Parse(json);
    if (!root) return false;

    cJSON *skills = cJSON_GetObjectItem(root, "skills");
    if (!skills) {
        cJSON_Delete(root);
        return false;
    }

    bool meets = false;
    cJSON *skill = NULL;
    cJSON_ArrayForEach(skill, skills) {
        cJSON *id_item = cJSON_GetObjectItem(skill, "id");
        if (id_item && id_item->valueint == (int)skill_id) {
            cJSON *level_req = cJSON_GetObjectItem(skill, "profession_level_req");
            if (level_req && profession_level < (uint8_t)level_req->valueint) {
                break;
            }

            cJSON *prof_req = cJSON_GetObjectItem(skill, "profession_req");
            if (!check_profession_req(pet, prof_req)) {
                break;
            }

            cJSON *stat_req = cJSON_GetObjectItem(skill, "stat_req");
            if (!check_stat_req(pet, stat_req)) {
                break;
            }

            meets = true;
            break;
        }
    }

    cJSON_Delete(root);
    return meets;
}

bool perk_check_requirements(pet_t *pet, uint8_t perk_id, uint8_t profession_level)
{
    if (pet == NULL) return false;
    if (perk_is_known(pet, perk_id)) return false;

    const char *json = storage_get_game_tables_json();
    if (!json) return false;

    cJSON *root = cJSON_Parse(json);
    if (!root) return false;

    cJSON *perks = cJSON_GetObjectItem(root, "perks");
    if (!perks) {
        cJSON_Delete(root);
        return false;
    }

    bool meets = false;
    cJSON *perk = NULL;
    cJSON_ArrayForEach(perk, perks) {
        cJSON *id_item = cJSON_GetObjectItem(perk, "id");
        if (id_item && id_item->valueint == (int)perk_id) {
            cJSON *level_req = cJSON_GetObjectItem(perk, "profession_level_req");
            if (level_req && profession_level < (uint8_t)level_req->valueint) {
                break;
            }

            cJSON *prof_req = cJSON_GetObjectItem(perk, "profession_req");
            if (!check_profession_req(pet, prof_req)) {
                break;
            }

            cJSON *stat_req = cJSON_GetObjectItem(perk, "stat_req");
            if (!check_stat_req(pet, stat_req)) {
                break;
            }

            meets = true;
            break;
        }
    }

    cJSON_Delete(root);
    return meets;
}

void skills_perks_get_available(pet_t *pet, uint8_t profession_level, learn_candidate_t *candidates, uint8_t *count)
{
    if (pet == NULL || candidates == NULL || count == NULL) {
        *count = 0;
        return;
    }

    *count = 0;

    const char *json = storage_get_game_tables_json();
    if (!json) return;

    cJSON *root = cJSON_Parse(json);
    if (!root) return;

    cJSON *skills = cJSON_GetObjectItem(root, "skills");
    if (skills && cJSON_IsArray(skills)) {
        cJSON *skill = NULL;
        cJSON_ArrayForEach(skill, skills) {
            cJSON *id_item = cJSON_GetObjectItem(skill, "id");
            if (!id_item) continue;

            uint8_t skill_id = (uint8_t)id_item->valueint;
            if (!skill_check_requirements(pet, skill_id, profession_level)) continue;

            cJSON *dp_cost = cJSON_GetObjectItem(skill, "dp_cost");
            cJSON *success_dc = cJSON_GetObjectItem(skill, "success_dc");
            cJSON *intent_item = cJSON_GetObjectItem(skill, "intent_type");

            if (pet->dp >= (uint32_t)(dp_cost ? dp_cost->valueint : 5)) {
                candidates[*count].type = LEARN_TYPE_SKILL;
                candidates[*count].id = skill_id;
                candidates[*count].intent_type = 0;
                if (intent_item && cJSON_IsString(intent_item)) {
                    const char *intent_str = intent_item->valuestring;
                    if (strcmp(intent_str, "attack") == 0) candidates[*count].intent_type = 0;
                    else if (strcmp(intent_str, "defend") == 0) candidates[*count].intent_type = 1;
                    else if (strcmp(intent_str, "heal") == 0) candidates[*count].intent_type = 2;
                    else if (strcmp(intent_str, "magic") == 0) candidates[*count].intent_type = 3;
                    else if (strcmp(intent_str, "support") == 0) candidates[*count].intent_type = 4;
                    else if (strcmp(intent_str, "flee") == 0) candidates[*count].intent_type = 5;
                }
                candidates[*count].dp_cost = dp_cost ? (uint8_t)dp_cost->valueint : 5;
                candidates[*count].success_dc = success_dc ? (uint8_t)success_dc->valueint : 10;
                (*count)++;

                if (*count >= 32) break;
            }
        }
    }

    cJSON *perks = cJSON_GetObjectItem(root, "perks");
    if (perks && cJSON_IsArray(perks) && *count < 32) {
        cJSON *perk = NULL;
        cJSON_ArrayForEach(perk, perks) {
            cJSON *id_item = cJSON_GetObjectItem(perk, "id");
            if (!id_item) continue;

            uint8_t perk_id = (uint8_t)id_item->valueint;
            if (!perk_check_requirements(pet, perk_id, profession_level)) continue;

            cJSON *dp_cost = cJSON_GetObjectItem(perk, "dp_cost");
            cJSON *success_dc = cJSON_GetObjectItem(perk, "success_dc");

            if (pet->dp >= (uint32_t)(dp_cost ? dp_cost->valueint : 5)) {
                candidates[*count].type = LEARN_TYPE_PERK;
                candidates[*count].id = perk_id;
                candidates[*count].dp_cost = dp_cost ? (uint8_t)dp_cost->valueint : 5;
                candidates[*count].success_dc = success_dc ? (uint8_t)success_dc->valueint : 10;
                (*count)++;

                if (*count >= 32) break;
            }
        }
    }

    cJSON_Delete(root);
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
                pet->skill_count++;
                ESP_LOGI(TAG, "Skill %d learned! (intent=%d, roll=%d, DC=%d)", candidate->id, candidate->intent_type, roll, candidate->success_dc);
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
