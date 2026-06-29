#include "resources_engine.h"
#include "game_tables_structs.h"
#include "storage_task.h"
#include "spi_bus.h"
#include "esp_log.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "RESOURCES";

void resources_init(void)
{
}

void resources_apply_profession(pet_t *pet)
{
    if (pet == NULL || pet->profession == PROF_NONE) return;

    memset(&pet->spell_slots, 0, sizeof(spell_slots_t));
    memset(pet->spell_slots_max, 0, sizeof(pet->spell_slots_max));
    pet->action_surge_uses = 0;
    pet->action_surge_max = 0;
    pet->indomitable_uses = 0;
    pet->indomitable_max = 0;
    pet->second_wind_uses = 0;
    pet->superiority_dice = 0;
    pet->superiority_dice_max = 0;
    pet->superiority_dice_size = 0;
    pet->sneak_attack_dice = 0;
    pet->arcane_recovery_used = 0;
    pet->ability_points_max = 3;
    pet->ability_points = pet->ability_points_max;

    resources_apply_level(pet, 1);
}

void resources_apply_level(pet_t *pet, uint8_t profession_level)
{
    if (pet == NULL || pet->profession == PROF_NONE) return;

    if (profession_level == 0 || profession_level > 20) {
        ESP_LOGW(TAG, "Invalid profession level %d", profession_level);
        return;
    }

    FILE *f = fopen("/sdcard/DATA/TABLES/resources.bin", "rb");
    if (!f) {
        ESP_LOGW(TAG, "resources.bin not found");
        return;
    }

    if (fseek(f, (profession_level - 1) * sizeof(resource_record_t), SEEK_SET) != 0) {
        fclose(f);
        return;
    }

    resource_record_t rec;
    if (fread(&rec, sizeof(resource_record_t), 1, f) != 1) {
        fclose(f);
        return;
    }
    fclose(f);

    switch (pet->profession) {
        case PROF_WARRIOR:
            pet->action_surge_max = rec.warrior.action_surge_max;
            pet->indomitable_max = rec.warrior.indomitable_max;
            pet->superiority_dice_max = rec.warrior.superiority_dice_max;
            pet->superiority_dice_size = rec.warrior.superiority_dice_size;
            break;
        case PROF_MAGE:
            for (int i = 0; i < 9; i++) {
                pet->spell_slots_max[i] = rec.mage.slots[i];
                pet->spell_slots.slots[i] = pet->spell_slots_max[i];
            }
            break;
        case PROF_ROGUE:
            pet->sneak_attack_dice = rec.rogue.sneak_attack_dice;
            break;
    }

    pet->action_surge_uses = pet->action_surge_max;
    pet->indomitable_uses = pet->indomitable_max;
    pet->second_wind_uses = 1;
    pet->superiority_dice = pet->superiority_dice_max;

    ESP_LOGI(TAG, "Applied resources for %s level %d", 
             pet->profession == PROF_WARRIOR ? "Warrior" :
             pet->profession == PROF_MAGE ? "Mage" : "Rogue",
             profession_level);
}

void resources_recover_short_rest(pet_t *pet)
{
    if (pet == NULL) return;

    if (pet->profession == PROF_WARRIOR) {
        pet->action_surge_uses = pet->action_surge_max;
        pet->second_wind_uses = 1;
        pet->superiority_dice = pet->superiority_dice_max;
    }

    pet->ability_points = pet->ability_points_max;

    ESP_LOGI(TAG, "Short rest resources recovered (ability_points: %d)", pet->ability_points);
}

void resources_recover_long_rest(pet_t *pet)
{
    if (pet == NULL) return;

    resources_recover_short_rest(pet);

    if (pet->profession == PROF_WARRIOR) {
        pet->indomitable_uses = pet->indomitable_max;
    }

    if (pet->profession == PROF_MAGE) {
        for (int i = 0; i < 9; i++) {
            pet->spell_slots.slots[i] = pet->spell_slots_max[i];
        }
        pet->arcane_recovery_used = 0;
    }

    ESP_LOGI(TAG, "Long rest resources recovered");
}
