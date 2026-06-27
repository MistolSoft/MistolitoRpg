#include "profession_engine.h"
#include "rules.h"
#include "storage_task.h"
#include "esp_log.h"
#include "esp_random.h"
#include "cJSON.h"
#include <string.h>

static const char *TAG = "PROF";

static uint8_t roll_d20(void)
{
    return (esp_random() % 20) + 1;
}

bool profession_check_requirements(pet_t *pet, uint8_t profession_id)
{
    if (pet == NULL || profession_id == 0) {
        return false;
    }

    const char *json = storage_get_game_tables_json();
    if (!json) {
        return false;
    }

    cJSON *root = cJSON_Parse(json);
    if (!root) {
        return false;
    }

    cJSON *professions = cJSON_GetObjectItem(root, "professions");
    if (!professions) {
        cJSON_Delete(root);
        return false;
    }

    bool meets_requirements = false;
    cJSON *prof = NULL;
    cJSON_ArrayForEach(prof, professions) {
        cJSON *id_item = cJSON_GetObjectItem(prof, "id");
        if (id_item && id_item->valueint == profession_id) {
            cJSON *req = cJSON_GetObjectItem(prof, "req");
            if (!req) {
                meets_requirements = true;
                break;
            }

            meets_requirements = true;

            cJSON *str_req = cJSON_GetObjectItem(req, "str");
            if (str_req && pet->str < str_req->valueint) {
                meets_requirements = false;
            }

            cJSON *dex_req = cJSON_GetObjectItem(req, "dex");
            if (dex_req && pet->dex < dex_req->valueint) {
                meets_requirements = false;
            }

            cJSON *con_req = cJSON_GetObjectItem(req, "con");
            if (con_req && pet->con < con_req->valueint) {
                meets_requirements = false;
            }

            cJSON *int_req = cJSON_GetObjectItem(req, "int");
            if (int_req && pet->intel < int_req->valueint) {
                meets_requirements = false;
            }

            cJSON *wis_req = cJSON_GetObjectItem(req, "wis");
            if (wis_req && pet->wis < wis_req->valueint) {
                meets_requirements = false;
            }

            cJSON *cha_req = cJSON_GetObjectItem(req, "cha");
            if (cha_req && pet->cha < cha_req->valueint) {
                meets_requirements = false;
            }

            break;
        }
    }

    cJSON_Delete(root);
    return meets_requirements;
}

bool profession_try_change(pet_t *pet, uint8_t new_profession_id)
{
    if (pet == NULL) {
        return false;
    }

    if (pet->profession != PROF_NONE) {
        ESP_LOGI(TAG, "Pet already has profession %d", pet->profession);
        return false;
    }

    if (!profession_check_requirements(pet, new_profession_id)) {
        ESP_LOGI(TAG, "Does not meet requirements for profession %d", new_profession_id);
        return false;
    }

    const char *json = storage_get_game_tables_json();
    if (!json) {
        return false;
    }

    cJSON *root = cJSON_Parse(json);
    if (!root) {
        return false;
    }

    cJSON *professions = cJSON_GetObjectItem(root, "professions");
    cJSON *prof = NULL;
    int dp_cost = 10;
    int success_dc = PROF_CHANGE_DC;

    cJSON_ArrayForEach(prof, professions) {
        cJSON *id_item = cJSON_GetObjectItem(prof, "id");
        if (id_item && id_item->valueint == new_profession_id) {
            cJSON *dp_item = cJSON_GetObjectItem(prof, "dp_cost");
            if (dp_item) dp_cost = dp_item->valueint;

            cJSON *dc_item = cJSON_GetObjectItem(prof, "success_dc");
            if (dc_item) success_dc = dc_item->valueint;
            break;
        }
    }
    cJSON_Delete(root);

    if (pet->dp < (uint32_t)dp_cost) {
        ESP_LOGI(TAG, "Not enough DP: have %lu, need %d", pet->dp, dp_cost);
        return false;
    }

    uint8_t roll = roll_d20();
    bool success = (roll >= success_dc);

    pet->dp -= dp_cost;

    if (success) {
        pet->profession = new_profession_id;
        pet->profession_level = 1;
        storage_apply_profession_data(pet, new_profession_id);
        ESP_LOGI(TAG, "Profession changed to %d! (roll=%d, DC=%d)", new_profession_id, roll, success_dc);
        return true;
    } else {
        ESP_LOGI(TAG, "Profession change failed (roll=%d, DC=%d), DP consumed", roll, success_dc);
        return false;
    }
}

void profession_increment_level(pet_t *pet)
{
    if (pet == NULL) {
        return;
    }

    if (pet->profession == PROF_NONE) {
        return;
    }

    pet->profession_level++;

    if (pet->profession_level > PROF_LEVEL_MAX) {
        pet->profession_level = PROF_LEVEL_CYCLE_RESET;
        ESP_LOGI(TAG, "Profession level cycle reset to 1");
    }

    ESP_LOGI(TAG, "Profession level: %d", pet->profession_level);
}

bool profession_try_stat_increase(pet_t *pet, uint8_t stat_idx, bool has_class_bonus)
{
    if (pet == NULL) {
        return false;
    }

    uint8_t roll1 = roll_d20();
    uint8_t final_roll;

    if (has_class_bonus) {
        uint8_t roll2 = roll_d20();
        final_roll = (roll1 > roll2) ? roll1 : roll2;
        ESP_LOGD(TAG, "Stat %d roll with advantage: %d (rolls %d, %d)", stat_idx, final_roll, roll1, roll2);
    } else {
        final_roll = roll1;
    }

    bool success = (final_roll >= STAT_DC_BASE);

    if (success) {
        ESP_LOGI(TAG, "Stat %d increase SUCCESS (roll=%d)", stat_idx, final_roll);
    } else {
        ESP_LOGI(TAG, "Stat %d increase FAILED (roll=%d)", stat_idx, final_roll);
    }

    return success;
}

void profession_get_bonus_stats(pet_t *pet, uint8_t stats[STAT_COUNT], uint8_t *count)
{
    if (pet == NULL || stats == NULL || count == NULL) {
        return;
    }

    *count = 0;

    if (pet->profession == PROF_NONE) {
        return;
    }

    const char *json = storage_get_game_tables_json();
    if (!json) {
        return;
    }

    cJSON *root = cJSON_Parse(json);
    if (!root) {
        return;
    }

    cJSON *professions = cJSON_GetObjectItem(root, "professions");
    cJSON *prof = NULL;

    cJSON_ArrayForEach(prof, professions) {
        cJSON *id_item = cJSON_GetObjectItem(prof, "id");
        if (id_item && id_item->valueint == pet->profession) {
            cJSON *bonus = cJSON_GetObjectItem(prof, "bonus_stats");
            if (bonus && cJSON_IsArray(bonus)) {
                cJSON *stat_name = NULL;
                cJSON_ArrayForEach(stat_name, bonus) {
                    if (strcmp(stat_name->valuestring, "str") == 0) stats[(*count)++] = STAT_STR;
                    else if (strcmp(stat_name->valuestring, "dex") == 0) stats[(*count)++] = STAT_DEX;
                    else if (strcmp(stat_name->valuestring, "con") == 0) stats[(*count)++] = STAT_CON;
                    else if (strcmp(stat_name->valuestring, "int") == 0) stats[(*count)++] = STAT_INT;
                    else if (strcmp(stat_name->valuestring, "wis") == 0) stats[(*count)++] = STAT_WIS;
                    else if (strcmp(stat_name->valuestring, "cha") == 0) stats[(*count)++] = STAT_CHA;
                }
            }
            break;
        }
    }

    cJSON_Delete(root);
}

void profession_apply_combat_stats(pet_t *pet)
{
if (pet == NULL || pet->profession == PROF_NONE) {
return;
}

const char *json = storage_get_game_tables_json();
if (!json) {
return;
}

cJSON *root = cJSON_Parse(json);
if (!root) {
return;
}

cJSON *professions = cJSON_GetObjectItem(root, "professions");
if (!professions) {
cJSON_Delete(root);
return;
}

cJSON *prof = NULL;
cJSON_ArrayForEach(prof, professions) {
cJSON *id_item = cJSON_GetObjectItem(prof, "id");
if (id_item && id_item->valueint == pet->profession) {
cJSON *hit_dice = cJSON_GetObjectItem(prof, "hit_dice");
cJSON *hp_per_level = cJSON_GetObjectItem(prof, "hp_per_level");
cJSON *damage_prog = cJSON_GetObjectItem(prof, "damage_progression");

if (hit_dice && hp_per_level) {
int8_t con_mod = rules_get_modifier(pet->con);
uint16_t hp_gain = (uint16_t)hp_per_level->valueint + (con_mod > 0 ? con_mod : 0);
pet->hp_max += hp_gain;
ESP_LOGI(TAG, "HP gain: +%d (hit_dice=d%d, hp_per_level=%d, con_mod=%d)", 
hp_gain, hit_dice->valueint, hp_per_level->valueint, con_mod);
}

if (damage_prog && cJSON_IsObject(damage_prog)) {
char level_key[8];
snprintf(level_key, sizeof(level_key), "%d", pet->profession_level);

cJSON *level_entry = cJSON_GetObjectItem(damage_prog, level_key);
if (level_entry) {
cJSON *dice_count = cJSON_GetObjectItem(level_entry, "dice_count");
if (dice_count) {
pet->combat.dice_count = (uint8_t)dice_count->valueint;
ESP_LOGI(TAG, "Dice count updated to %d", pet->combat.dice_count);
}
}
}

break;
}
}

cJSON_Delete(root);
}

void profession_apply_level_bonuses(pet_t *pet, uint8_t profession_level)
{
    if (pet == NULL || pet->profession == PROF_NONE) {
        return;
    }

    const char *prof_names[] = {"novice", "warrior", "mage", "rogue"};
    const char *prof_name = prof_names[pet->profession];

    const char *json = storage_get_game_tables_json();
    if (!json) {
        return;
    }

    cJSON *root = cJSON_Parse(json);
    if (!root) {
        return;
    }

    cJSON *level_tables = cJSON_GetObjectItem(root, "level_tables");
    if (!level_tables) {
        cJSON_Delete(root);
        return;
    }

    cJSON *prof_table = cJSON_GetObjectItem(level_tables, prof_name);
    if (!prof_table) {
        cJSON_Delete(root);
        return;
    }

    cJSON *damage_prog = cJSON_GetObjectItem(prof_table, "damage_progression");
    if (damage_prog && cJSON_IsArray(damage_prog)) {
        cJSON *entry = NULL;
        cJSON_ArrayForEach(entry, damage_prog) {
            cJSON *level_item = cJSON_GetObjectItem(entry, "level");
            if (level_item && level_item->valueint == profession_level) {
                cJSON *min_dmg = cJSON_GetObjectItem(entry, "min_damage");
                if (min_dmg) pet->bonuses.min_damage += min_dmg->valueint;

                cJSON *max_dmg = cJSON_GetObjectItem(entry, "max_damage");
                if (max_dmg) pet->bonuses.max_damage += max_dmg->valueint;

                cJSON *extra_dice = cJSON_GetObjectItem(entry, "extra_dice");
                if (extra_dice) pet->bonuses.extra_dice += extra_dice->valueint;

                cJSON *crit = cJSON_GetObjectItem(entry, "crit");
                if (crit) pet->bonuses.crit += crit->valueint;

                cJSON *sneak = cJSON_GetObjectItem(entry, "sneak_dice");
                if (sneak) pet->bonuses.sneak_dice += sneak->valueint;

                cJSON *skill_uses = cJSON_GetObjectItem(entry, "skill_uses");
                if (skill_uses) pet->bonuses.skill_uses += skill_uses->valueint;

                ESP_LOGI(TAG, "Applied damage progression at prof_level %d", profession_level);
                break;
            }
        }
    }

    cJSON_Delete(root);
}
