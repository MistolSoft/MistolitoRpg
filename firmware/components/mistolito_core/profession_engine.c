#include "profession_engine.h"
#include "game_tables_structs.h"
#include "rules.h"
#include "storage_task.h"
#include "esp_log.h"
#include "esp_random.h"
#include "cJSON.h"
#include <string.h>
#include <stdio.h>

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

    FILE *f = fopen("/sdcard/DATA/TABLES/professions.bin", "rb");
    if (!f) {
        return false;
    }

    profession_record_t prof;
    bool meets = false;
    while (fread(&prof, sizeof(profession_record_t), 1, f) == 1) {
        if (prof.id == profession_id) {
            meets = true;
            if (pet->str < prof.req_str) meets = false;
            if (pet->con < prof.req_con) meets = false;
            if (pet->dex < prof.req_dex) meets = false;
            if (pet->intel < prof.req_int) meets = false;
            if (pet->wis < prof.req_wis) meets = false;
            if (pet->cha < prof.req_cha) meets = false;
            break;
        }
    }
    fclose(f);
    return meets;
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

    FILE *f = fopen("/sdcard/DATA/TABLES/professions.bin", "rb");
    if (!f) {
        return false;
    }

    profession_record_t prof;
    int dp_cost = 10;
    int success_dc = PROF_CHANGE_DC;
    bool found = false;

    while (fread(&prof, sizeof(profession_record_t), 1, f) == 1) {
        if (prof.id == new_profession_id) {
            dp_cost = prof.dp_cost;
            success_dc = prof.success_dc;
            found = true;
            break;
        }
    }
    fclose(f);

    if (!found) {
        return false;
    }

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

    FILE *f = fopen("/sdcard/DATA/TABLES/professions.bin", "rb");
    if (!f) {
        return;
    }

    profession_record_t prof;
    while (fread(&prof, sizeof(profession_record_t), 1, f) == 1) {
        if (prof.id == pet->profession) {
            if (prof.bonus_str) stats[(*count)++] = STAT_STR;
            if (prof.bonus_con) stats[(*count)++] = STAT_CON;
            if (prof.bonus_dex) stats[(*count)++] = STAT_DEX;
            if (prof.bonus_int) stats[(*count)++] = STAT_INT;
            if (prof.bonus_wis) stats[(*count)++] = STAT_WIS;
            if (prof.bonus_cha) stats[(*count)++] = STAT_CHA;
            break;
        }
    }
    fclose(f);
}

static uint8_t profession_get_dice_count(uint8_t profession_id, uint8_t level)
{
    if (profession_id == PROF_WARRIOR || profession_id == PROF_ROGUE) {
        return 3 + (level + 1) / 6;
    }
    if (profession_id == PROF_NONE) {
        return 1 + (level + 1) / 6;
    }
    return 1;
}

void profession_apply_combat_stats(pet_t *pet)
{
    if (pet == NULL || pet->profession == PROF_NONE) {
        return;
    }

    FILE *f = fopen("/sdcard/DATA/TABLES/professions.bin", "rb");
    if (!f) {
        return;
    }

    profession_record_t prof;
    bool found = false;
    while (fread(&prof, sizeof(profession_record_t), 1, f) == 1) {
        if (prof.id == pet->profession) {
            int8_t con_mod = rules_get_modifier(pet->con);
            uint16_t hp_gain = (uint16_t)prof.hp_per_level + (con_mod > 0 ? con_mod : 0);
            pet->hp_max += hp_gain;
            ESP_LOGI(TAG, "HP gain: +%d (hit_dice=d%d, hp_per_level=%d, con_mod=%d)", 
                     hp_gain, prof.hit_dice, prof.hp_per_level, con_mod);
            found = true;
            break;
        }
    }
    fclose(f);

    if (found) {
        pet->combat.dice_count = profession_get_dice_count(pet->profession, pet->profession_level);
        ESP_LOGI(TAG, "Dice count updated to %d", pet->combat.dice_count);
    }
}

void profession_apply_level_bonuses(pet_t *pet, uint8_t profession_level)
{
    if (pet == NULL || pet->profession == PROF_NONE) {
        return;
    }

    FILE *f = fopen("/sdcard/DATA/TABLES/damage_progression.bin", "rb");
    if (!f) {
        return;
    }

    damage_progression_record_t rec;
    while (fread(&rec, sizeof(damage_progression_record_t), 1, f) == 1) {
        if (rec.profession == pet->profession && rec.level == profession_level) {
            pet->bonuses.min_damage += rec.min_damage;
            pet->bonuses.max_damage += rec.max_damage;
            pet->bonuses.extra_dice += rec.extra_dice;
            pet->bonuses.crit += rec.crit;
            pet->bonuses.sneak_dice += rec.sneak_dice;
            pet->bonuses.skill_uses += rec.skill_uses;
            ESP_LOGI(TAG, "Applied damage progression at prof_level %d", profession_level);
            break;
        }
    }
    fclose(f);
}
