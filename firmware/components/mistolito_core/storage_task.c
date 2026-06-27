#include "storage_task.h"
#include "mistolito.h"
#include "dna_engine.h"
#include "esp_log.h"
#include "esp_random.h"
#include "spi_bus.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdspi_host.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <sys/stat.h>
#include <errno.h>
#include <dirent.h>

static const char *TAG = "STORAGE";

#define SD_MISO 40
#define SD_MOSI 38
#define SD_CLK 39
#define SD_CS 41
#define MOUNT_POINT "/sdcard"

QueueHandle_t g_storage_queue = NULL;
static bool g_mounted = false;

static void replay_build_paths(void);

static replay_header_t g_replay_header;
static bool g_replay_initialized = false;
static uint8_t g_replay_num_actions = 0;
static char g_replay_dir[64];
static char g_replay_header_path[80];
static char g_replay_data_dir[64];

void storage_task_start(void)
{
    if (g_storage_queue == NULL) {
        g_storage_queue = xQueueCreate(STORAGE_QUEUE_SIZE, sizeof(storage_request_t));
    }

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SPI2_HOST;

    sdspi_device_config_t slot_cfg = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_cfg.gpio_cs = SD_CS;
    slot_cfg.host_id = SPI2_HOST;

    esp_vfs_fat_sdmmc_mount_config_t mount_cfg = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 4096
    };

    sdmmc_card_t *card = NULL;
    esp_err_t ret = esp_vfs_fat_sdspi_mount(MOUNT_POINT, &host, &slot_cfg, &mount_cfg, &card);

    if (ret == ESP_OK) {
        g_mounted = true;
        ESP_LOGI(TAG, "SD card mounted");
    } else {
        ESP_LOGE(TAG, "Mount failed: %s", esp_err_to_name(ret));
    }
}

void storage_task(void *arg)
{
    storage_request_t req;

    ESP_LOGI(TAG, "Storage task started");

    while (1) {
        if (xQueueReceive(g_storage_queue, &req, portMAX_DELAY) == pdTRUE) {
            if (!g_mounted) {
                ESP_LOGW(TAG, "Storage not mounted, skipping operation");
                continue;
            }

            spi_bus_lock();

            switch (req.operation) {
case STORAGE_OP_SAVE_PET_DELTA:
    {
        FILE *f = fopen(MOUNT_POINT "/BRAIN/PET/pet_data.json", "w");
        if (f) {
            fprintf(f, "{\"name\":\"%s\",\"level\":%d,\"profession_level\":%d,\"exp\":%lu,\"hp\":%d,\"hp_max\":%d,\"energy\":%d,\"profession\":%d,\"str\":%d,\"dex\":%d,\"con\":%d,\"int\":%d,\"wis\":%d,\"cha\":%d,\"dp\":%lu,\"enemies_killed\":%lu,\"lives\":%d,\"hp_rest_threshold\":%d,\"recovery_chance\":%d,\"base_ac\":%d,\"damage_dice\":%d,\"damage_bonus\":%d,\"dice_count\":%d,\"dna_salt\":%lu,\"skill_count\":%d,\"perk_count\":%d",
            req.pet.pet->name, req.pet.pet->level, req.pet.pet->profession_level, (unsigned long)req.pet.pet->exp,
            req.pet.pet->hp, req.pet.pet->hp_max, req.pet.pet->energy, req.pet.pet->profession,
            req.pet.pet->str, req.pet.pet->dex, req.pet.pet->con,
            req.pet.pet->intel, req.pet.pet->wis, req.pet.pet->cha,
            (unsigned long)req.pet.pet->dp, (unsigned long)req.pet.pet->enemies_killed, req.pet.pet->lives,
            req.pet.pet->rest.hp_rest_threshold, req.pet.pet->rest.recovery_chance,
            req.pet.pet->combat.base_ac, req.pet.pet->combat.damage_dice, req.pet.pet->combat.damage_bonus, req.pet.pet->combat.dice_count,
            (unsigned long)req.pet.pet->dna.salt, req.pet.pet->skill_count, req.pet.pet->perk_count);

            fprintf(f, ",\"skills\":[");
            for (uint8_t i = 0; i < req.pet.pet->skill_count; i++) {
                if (i > 0) fprintf(f, ",");
                fprintf(f, "{\"id\":%d,\"intent\":%d,\"uses\":%d}", req.pet.pet->skills[i].skill_id, req.pet.pet->skills[i].intent_type, req.pet.pet->skills[i].uses_remaining);
            }
            fprintf(f, "]");

            fprintf(f, ",\"perks\":[");
            for (uint8_t i = 0; i < req.pet.pet->perk_count; i++) {
                if (i > 0) fprintf(f, ",");
                fprintf(f, "{\"id\":%d}", req.pet.pet->perks[i].id);
}
fprintf(f, "]");

fprintf(f, ",\"spell_slots\":[");
for (uint8_t i = 0; i < 9; i++) {
if (i > 0) fprintf(f, ",");
fprintf(f, "%d", req.pet.pet->spell_slots.slots[i]);
}
fprintf(f, "]");

fprintf(f, ",\"spell_slots_max\":[");
for (uint8_t i = 0; i < 9; i++) {
if (i > 0) fprintf(f, ",");
fprintf(f, "%d", req.pet.pet->spell_slots_max[i]);
}
fprintf(f, "]");

fprintf(f, ",\"spells_known\":[");
for (uint8_t i = 0; i < req.pet.pet->spells_known_count; i++) {
if (i > 0) fprintf(f, ",");
fprintf(f, "{\"id\":\"%s\",\"level\":%d}", req.pet.pet->spells_known[i].id, req.pet.pet->spells_known[i].level);
}
fprintf(f, "]");

fprintf(f, ",\"cantrips_known\":%d,\"cantrips_max\":%d", req.pet.pet->cantrips_known, req.pet.pet->cantrips_max);

fprintf(f, ",\"action_surge_uses\":%d,\"action_surge_max\":%d", req.pet.pet->action_surge_uses, req.pet.pet->action_surge_max);
fprintf(f, ",\"indomitable_uses\":%d,\"indomitable_max\":%d", req.pet.pet->indomitable_uses, req.pet.pet->indomitable_max);
fprintf(f, ",\"second_wind_uses\":%d", req.pet.pet->second_wind_uses);
fprintf(f, ",\"superiority_dice\":%d,\"superiority_dice_max\":%d,\"superiority_dice_size\":%d",
req.pet.pet->superiority_dice, req.pet.pet->superiority_dice_max, req.pet.pet->superiority_dice_size);
fprintf(f, ",\"sneak_attack_dice\":%d", req.pet.pet->sneak_attack_dice);
fprintf(f, ",\"arcane_recovery_used\":%d", req.pet.pet->arcane_recovery_used);

fprintf(f, ",\"maneuvers\":[");
        for (uint8_t i = 0; i < req.pet.pet->maneuver_count; i++) {
            if (i > 0) fprintf(f, ",");
            fprintf(f, "{\"id\":%d}", req.pet.pet->maneuvers[i].maneuver_id);
        }
        fprintf(f, "]");

fprintf(f, ",\"ability_points\":%d,\"ability_points_max\":%d", req.pet.pet->ability_points, req.pet.pet->ability_points_max);

fprintf(f, "}");

fclose(f);
ESP_LOGI(TAG, "Pet saved (dirty=0x%04X)", req.pet.dirty_flags);
        }
    }
    break;

            case STORAGE_OP_LOAD_PET:
            {
                FILE *f = fopen(MOUNT_POINT "/BRAIN/PET/pet_data.json", "r");
                if (f) {
                    char buf[512];
                    size_t len = fread(buf, 1, sizeof(buf) - 1, f);
                    buf[len] = '\0';
                    fclose(f);

                    cJSON *root = cJSON_Parse(buf);
                    if (root) {
                        cJSON *item;
if ((item = cJSON_GetObjectItem(root, "name"))) {
    strncpy(req.pet.pet->name, item->valuestring, PET_NAME_MAX_LEN - 1);
}
if ((item = cJSON_GetObjectItem(root, "level"))) req.pet.pet->level = item->valueint;
if ((item = cJSON_GetObjectItem(root, "profession_level"))) req.pet.pet->profession_level = item->valueint;
if ((item = cJSON_GetObjectItem(root, "exp"))) req.pet.pet->exp = item->valueint;
                        if ((item = cJSON_GetObjectItem(root, "hp"))) req.pet.pet->hp = item->valueint;
                        if ((item = cJSON_GetObjectItem(root, "hp_max"))) req.pet.pet->hp_max = item->valueint;
                        if ((item = cJSON_GetObjectItem(root, "energy"))) req.pet.pet->energy = item->valueint;
                        if ((item = cJSON_GetObjectItem(root, "str"))) req.pet.pet->str = item->valueint;
                        if ((item = cJSON_GetObjectItem(root, "dex"))) req.pet.pet->dex = item->valueint;
                        if ((item = cJSON_GetObjectItem(root, "con"))) req.pet.pet->con = item->valueint;
                        if ((item = cJSON_GetObjectItem(root, "int"))) req.pet.pet->intel = item->valueint;
                        if ((item = cJSON_GetObjectItem(root, "wis"))) req.pet.pet->wis = item->valueint;
            if ((item = cJSON_GetObjectItem(root, "cha"))) req.pet.pet->cha = item->valueint;
            if ((item = cJSON_GetObjectItem(root, "dp"))) req.pet.pet->dp = item->valueint;
            if ((item = cJSON_GetObjectItem(root, "enemies_killed"))) req.pet.pet->enemies_killed = item->valueint;
            if ((item = cJSON_GetObjectItem(root, "lives"))) req.pet.pet->lives = item->valueint;
            if ((item = cJSON_GetObjectItem(root, "hp_rest_threshold"))) req.pet.pet->rest.hp_rest_threshold = item->valueint;
            if ((item = cJSON_GetObjectItem(root, "recovery_chance"))) req.pet.pet->rest.recovery_chance = item->valueint;
            if ((item = cJSON_GetObjectItem(root, "base_ac"))) req.pet.pet->combat.base_ac = item->valueint;
            if ((item = cJSON_GetObjectItem(root, "damage_dice"))) req.pet.pet->combat.damage_dice = item->valueint;
            if ((item = cJSON_GetObjectItem(root, "damage_bonus"))) req.pet.pet->combat.damage_bonus = item->valueint;
if ((item = cJSON_GetObjectItem(root, "dice_count"))) req.pet.pet->combat.dice_count = item->valueint;

    cJSON *skills_arr = cJSON_GetObjectItem(root, "skills");
    if (skills_arr && cJSON_IsArray(skills_arr)) {
        cJSON *skill_item = NULL;
        cJSON_ArrayForEach(skill_item, skills_arr) {
            if (req.pet.pet->skill_count < MAX_SKILLS) {
                cJSON *id_item = cJSON_GetObjectItem(skill_item, "id");
                cJSON *uses_item = cJSON_GetObjectItem(skill_item, "uses");
                cJSON *intent_item = cJSON_GetObjectItem(skill_item, "intent");
                if (id_item) {
                    req.pet.pet->skills[req.pet.pet->skill_count].skill_id = (uint8_t)id_item->valueint;
                    req.pet.pet->skills[req.pet.pet->skill_count].intent_type = intent_item ? (uint8_t)intent_item->valueint : 0;
                    req.pet.pet->skills[req.pet.pet->skill_count].uses_remaining = uses_item ? (uint8_t)uses_item->valueint : 3;
                    req.pet.pet->skills[req.pet.pet->skill_count].uses_max = 3;
                    req.pet.pet->skill_count++;
                }
            }
        }
    }

cJSON *perks_arr = cJSON_GetObjectItem(root, "perks");
if (perks_arr && cJSON_IsArray(perks_arr)) {
cJSON *perk_item = NULL;
cJSON_ArrayForEach(perk_item, perks_arr) {
if (req.pet.pet->perk_count < MAX_PERKS) {
cJSON *id_item = cJSON_GetObjectItem(perk_item, "id");
if (id_item) {
req.pet.pet->perks[req.pet.pet->perk_count].id = (uint8_t)id_item->valueint;
req.pet.pet->perk_count++;
}
}
}
}

cJSON *spell_slots_arr = cJSON_GetObjectItem(root, "spell_slots");
if (spell_slots_arr && cJSON_IsArray(spell_slots_arr)) {
uint8_t idx = 0;
cJSON *slot_item = NULL;
cJSON_ArrayForEach(slot_item, spell_slots_arr) {
if (idx < 9) req.pet.pet->spell_slots.slots[idx++] = (uint8_t)slot_item->valueint;
}
}

cJSON *spell_slots_max_arr = cJSON_GetObjectItem(root, "spell_slots_max");
if (spell_slots_max_arr && cJSON_IsArray(spell_slots_max_arr)) {
uint8_t idx = 0;
cJSON *slot_item = NULL;
cJSON_ArrayForEach(slot_item, spell_slots_max_arr) {
if (idx < 9) req.pet.pet->spell_slots_max[idx++] = (uint8_t)slot_item->valueint;
}
}

cJSON *spells_known_arr = cJSON_GetObjectItem(root, "spells_known");
if (spells_known_arr && cJSON_IsArray(spells_known_arr)) {
cJSON *spell_item = NULL;
cJSON_ArrayForEach(spell_item, spells_known_arr) {
if (req.pet.pet->spells_known_count < MAX_SPELLS_KNOWN) {
cJSON *id_item = cJSON_GetObjectItem(spell_item, "id");
cJSON *level_item = cJSON_GetObjectItem(spell_item, "level");
if (id_item) {
strncpy(req.pet.pet->spells_known[req.pet.pet->spells_known_count].id, id_item->valuestring, sizeof(req.pet.pet->spells_known[0].id) - 1);
req.pet.pet->spells_known[req.pet.pet->spells_known_count].level = level_item ? (uint8_t)level_item->valueint : 0;
req.pet.pet->spells_known_count++;
}
}
}
}

if ((item = cJSON_GetObjectItem(root, "cantrips_known"))) req.pet.pet->cantrips_known = item->valueint;
if ((item = cJSON_GetObjectItem(root, "cantrips_max"))) req.pet.pet->cantrips_max = item->valueint;
if ((item = cJSON_GetObjectItem(root, "action_surge_uses"))) req.pet.pet->action_surge_uses = item->valueint;
if ((item = cJSON_GetObjectItem(root, "action_surge_max"))) req.pet.pet->action_surge_max = item->valueint;
if ((item = cJSON_GetObjectItem(root, "indomitable_uses"))) req.pet.pet->indomitable_uses = item->valueint;
if ((item = cJSON_GetObjectItem(root, "indomitable_max"))) req.pet.pet->indomitable_max = item->valueint;
if ((item = cJSON_GetObjectItem(root, "second_wind_uses"))) req.pet.pet->second_wind_uses = item->valueint;
if ((item = cJSON_GetObjectItem(root, "superiority_dice"))) req.pet.pet->superiority_dice = item->valueint;
if ((item = cJSON_GetObjectItem(root, "superiority_dice_max"))) req.pet.pet->superiority_dice_max = item->valueint;
if ((item = cJSON_GetObjectItem(root, "superiority_dice_size"))) req.pet.pet->superiority_dice_size = item->valueint;
if ((item = cJSON_GetObjectItem(root, "sneak_attack_dice"))) req.pet.pet->sneak_attack_dice = item->valueint;
if ((item = cJSON_GetObjectItem(root, "arcane_recovery_used"))) req.pet.pet->arcane_recovery_used = item->valueint;

cJSON *maneuvers_arr = cJSON_GetObjectItem(root, "maneuvers");
                if (maneuvers_arr && cJSON_IsArray(maneuvers_arr)) {
                    cJSON *maneuver_item = NULL;
                    cJSON_ArrayForEach(maneuver_item, maneuvers_arr) {
                        if (req.pet.pet->maneuver_count < MAX_MANEUVERS_KNOWN) {
                            cJSON *id_item = cJSON_GetObjectItem(maneuver_item, "id");
                            if (id_item) {
                                req.pet.pet->maneuvers[req.pet.pet->maneuver_count].maneuver_id = (uint8_t)id_item->valueint;
                                req.pet.pet->maneuver_count++;
                            }
                        }
                    }
                }

if ((item = cJSON_GetObjectItem(root, "ability_points"))) req.pet.pet->ability_points = item->valueint;
                if ((item = cJSON_GetObjectItem(root, "ability_points_max"))) req.pet.pet->ability_points_max = item->valueint;

cJSON_Delete(root);
ESP_LOGI(TAG, "Pet loaded: %s Lv.%d (skills=%d, perks=%d, spells=%d, maneuvers=%d)", 
req.pet.pet->name, req.pet.pet->level, req.pet.pet->skill_count, req.pet.pet->perk_count,
req.pet.pet->spells_known_count, req.pet.pet->maneuver_count);
}
}
}
break;

            case STORAGE_OP_LOAD_TABLES:
                break;

            case STORAGE_OP_REPLAY_INIT:
            {
                mkdir(g_replay_dir, 0755);
                mkdir(EPISODES_DIR, 0755);
                mkdir(CHECKPOINTS_DIR, 0755);
                mkdir(EXPLORATION_DIR, 0755);

                FILE *f = fopen(g_replay_header_path, "rb");
                if (f) {
                    fread(&g_replay_header, sizeof(replay_header_t), 1, f);
                    fclose(f);
                    if (g_replay_header.magic != REPLAY_MAGIC) {
                        ESP_LOGW(TAG, "Replay header invalid, recreating");
                        memset(&g_replay_header, 0, sizeof(replay_header_t));
                    } else if (g_replay_header.num_actions != 0) {
                        g_replay_num_actions = g_replay_header.num_actions;
                        replay_build_paths();
                    }
                } else {
                    memset(&g_replay_header, 0, sizeof(replay_header_t));
                    g_replay_header.magic = REPLAY_MAGIC;
                    g_replay_header.version = REPLAY_VERSION;
                    g_replay_header.num_actions = g_replay_num_actions;
                    ESP_LOGI(TAG, "Creating new replay header for a%u", g_replay_num_actions);
                }

                g_replay_initialized = true;
                ESP_LOGI(TAG, "Replay task init: a%u, %lu chunks, %lu transitions",
                         g_replay_num_actions,
                         (unsigned long)g_replay_header.total_chunks,
                         (unsigned long)g_replay_header.total_transitions);
                break;
            }

            case STORAGE_OP_REPLAY_APPEND:
            {
                if (!g_replay_initialized) {
                    ESP_LOGW(TAG, "Replay not initialized, skipping append");
                    break;
                }

                replay_transition_t *tr = &req.replay.transition;

                if (g_replay_header.current_chunk_transitions >= REPLAY_TRANSITIONS_PER_CHUNK) {
                    g_replay_header.total_chunks++;
                    g_replay_header.current_chunk_transitions = 0;
                }

                if (g_replay_header.total_chunks == 0) {
                    g_replay_header.total_chunks = 1;
                }

                char chunk_path[128];
                snprintf(chunk_path, sizeof(chunk_path), "%s/chunk_%04lu.bin", g_replay_dir,
                         (unsigned long)g_replay_header.total_chunks);

                FILE *f = fopen(chunk_path, "ab");
                if (f) {
                    fwrite(tr, sizeof(replay_transition_t), 1, f);
                    fclose(f);

                    g_replay_header.current_chunk_transitions++;
                    g_replay_header.total_transitions++;

                    FILE *hf = fopen(g_replay_header_path, "wb");
                    if (hf) {
                        fwrite(&g_replay_header, sizeof(replay_header_t), 1, hf);
                        fclose(hf);
                    }
                } else {
                    ESP_LOGE(TAG, "Failed to open chunk for append: %s (errno=%d)", chunk_path, errno);
                }
                break;
            }

            case STORAGE_OP_REPLAY_READ:
            {
                if (!g_replay_initialized) break;

                uint32_t chunk_index = req.replay_read.chunk_index;
                uint32_t *out_count = req.replay_read.out_count;

                char chunk_path[128];
                snprintf(chunk_path, sizeof(chunk_path), "%s/chunk_%04lu.bin", g_replay_dir,
                         (unsigned long)chunk_index);

                FILE *f = fopen(chunk_path, "rb");
                if (f) {
                    fseek(f, 0, SEEK_END);
                    long size = ftell(f);
                    fseek(f, 0, SEEK_SET);

                    *out_count = size / sizeof(replay_transition_t);
                    ESP_LOGI(TAG, "Read chunk %lu: %lu transitions (%ld bytes)",
                             (unsigned long)chunk_index, (unsigned long)*out_count, size);
                    fclose(f);
                } else {
                    *out_count = 0;
                    ESP_LOGW(TAG, "Chunk %lu not found", (unsigned long)chunk_index);
                }
                break;
            }
            }

            spi_bus_unlock();
        }
    }
}

void storage_save_pet_delta(uint8_t dirty_flags, pet_t *pet)
{
    if (g_storage_queue == NULL) return;

    storage_request_t req = {
        .pet = {
            .operation = STORAGE_OP_SAVE_PET_DELTA,
            .dirty_flags = dirty_flags,
            .pet = pet
        }
    };
    xQueueSend(g_storage_queue, &req, 0);
}

bool storage_load_pet(pet_t *pet)
{
    if (!g_mounted) return false;

    spi_bus_lock();

    FILE *f = fopen(MOUNT_POINT "/BRAIN/PET/pet_data.json", "r");
    if (!f) {
        spi_bus_unlock();
        return false;
    }

    char buf[512];
    size_t len = fread(buf, 1, sizeof(buf) - 1, f);
    buf[len] = '\0';
    fclose(f);

    spi_bus_unlock();

    cJSON *root = cJSON_Parse(buf);
    if (!root) return false;

cJSON *item;
if ((item = cJSON_GetObjectItem(root, "name"))) strncpy(pet->name, item->valuestring, PET_NAME_MAX_LEN - 1);
if ((item = cJSON_GetObjectItem(root, "level"))) pet->level = item->valueint;
if ((item = cJSON_GetObjectItem(root, "profession_level"))) pet->profession_level = item->valueint;
if ((item = cJSON_GetObjectItem(root, "exp"))) pet->exp = item->valueint;
    if ((item = cJSON_GetObjectItem(root, "hp"))) pet->hp = item->valueint;
    if ((item = cJSON_GetObjectItem(root, "hp_max"))) pet->hp_max = item->valueint;
    if ((item = cJSON_GetObjectItem(root, "energy"))) pet->energy = item->valueint;
    if ((item = cJSON_GetObjectItem(root, "str"))) pet->str = item->valueint;
    if ((item = cJSON_GetObjectItem(root, "dex"))) pet->dex = item->valueint;
    if ((item = cJSON_GetObjectItem(root, "con"))) pet->con = item->valueint;
    if ((item = cJSON_GetObjectItem(root, "int"))) pet->intel = item->valueint;
    if ((item = cJSON_GetObjectItem(root, "wis"))) pet->wis = item->valueint;
    if ((item = cJSON_GetObjectItem(root, "cha"))) pet->cha = item->valueint;
    if ((item = cJSON_GetObjectItem(root, "dp"))) pet->dp = item->valueint;
    if ((item = cJSON_GetObjectItem(root, "enemies_killed"))) pet->enemies_killed = item->valueint;
    if ((item = cJSON_GetObjectItem(root, "lives"))) pet->lives = item->valueint;
    if ((item = cJSON_GetObjectItem(root, "hp_rest_threshold"))) pet->rest.hp_rest_threshold = item->valueint;
    if ((item = cJSON_GetObjectItem(root, "recovery_chance"))) pet->rest.recovery_chance = item->valueint;
    if ((item = cJSON_GetObjectItem(root, "base_ac"))) pet->combat.base_ac = item->valueint;
    if ((item = cJSON_GetObjectItem(root, "damage_dice"))) pet->combat.damage_dice = item->valueint;
    if ((item = cJSON_GetObjectItem(root, "damage_bonus"))) pet->combat.damage_bonus = item->valueint;
if ((item = cJSON_GetObjectItem(root, "dice_count"))) pet->combat.dice_count = item->valueint;

cJSON *skills_arr = cJSON_GetObjectItem(root, "skills");
if (skills_arr && cJSON_IsArray(skills_arr)) {
    cJSON *skill_item = NULL;
    cJSON_ArrayForEach(skill_item, skills_arr) {
        if (pet->skill_count < MAX_SKILLS) {
            cJSON *id_item = cJSON_GetObjectItem(skill_item, "id");
            cJSON *uses_item = cJSON_GetObjectItem(skill_item, "uses");
            cJSON *intent_item = cJSON_GetObjectItem(skill_item, "intent");
            if (id_item) {
                pet->skills[pet->skill_count].skill_id = (uint8_t)id_item->valueint;
                pet->skills[pet->skill_count].intent_type = intent_item ? (uint8_t)intent_item->valueint : 0;
                pet->skills[pet->skill_count].uses_remaining = uses_item ? (uint8_t)uses_item->valueint : 3;
                pet->skills[pet->skill_count].uses_max = 3;
                pet->skill_count++;
            }
        }
    }
}

cJSON *perks_arr = cJSON_GetObjectItem(root, "perks");
if (perks_arr && cJSON_IsArray(perks_arr)) {
cJSON *perk_item = NULL;
cJSON_ArrayForEach(perk_item, perks_arr) {
if (pet->perk_count < MAX_PERKS) {
cJSON *id_item = cJSON_GetObjectItem(perk_item, "id");
if (id_item) {
pet->perks[pet->perk_count].id = (uint8_t)id_item->valueint;
pet->perk_count++;
}
}
}
}

cJSON *spell_slots_arr = cJSON_GetObjectItem(root, "spell_slots");
if (spell_slots_arr && cJSON_IsArray(spell_slots_arr)) {
uint8_t idx = 0;
cJSON *slot_item = NULL;
cJSON_ArrayForEach(slot_item, spell_slots_arr) {
if (idx < 9) {
pet->spell_slots.slots[idx++] = (uint8_t)slot_item->valueint;
}
}
}

cJSON *spell_slots_max_arr = cJSON_GetObjectItem(root, "spell_slots_max");
if (spell_slots_max_arr && cJSON_IsArray(spell_slots_max_arr)) {
uint8_t idx = 0;
cJSON *slot_item = NULL;
cJSON_ArrayForEach(slot_item, spell_slots_max_arr) {
if (idx < 9) {
pet->spell_slots_max[idx++] = (uint8_t)slot_item->valueint;
}
}
}

cJSON *spells_known_arr = cJSON_GetObjectItem(root, "spells_known");
if (spells_known_arr && cJSON_IsArray(spells_known_arr)) {
cJSON *spell_item = NULL;
cJSON_ArrayForEach(spell_item, spells_known_arr) {
if (pet->spells_known_count < MAX_SPELLS_KNOWN) {
cJSON *id_item = cJSON_GetObjectItem(spell_item, "id");
cJSON *level_item = cJSON_GetObjectItem(spell_item, "level");
if (id_item) {
strncpy(pet->spells_known[pet->spells_known_count].id, id_item->valuestring, sizeof(pet->spells_known[0].id) - 1);
pet->spells_known[pet->spells_known_count].id[sizeof(pet->spells_known[0].id) - 1] = '\0';
pet->spells_known[pet->spells_known_count].level = level_item ? (uint8_t)level_item->valueint : 0;
pet->spells_known_count++;
}
}
}
}

if ((item = cJSON_GetObjectItem(root, "cantrips_known"))) pet->cantrips_known = item->valueint;
if ((item = cJSON_GetObjectItem(root, "cantrips_max"))) pet->cantrips_max = item->valueint;
if ((item = cJSON_GetObjectItem(root, "action_surge_uses"))) pet->action_surge_uses = item->valueint;
if ((item = cJSON_GetObjectItem(root, "action_surge_max"))) pet->action_surge_max = item->valueint;
if ((item = cJSON_GetObjectItem(root, "indomitable_uses"))) pet->indomitable_uses = item->valueint;
if ((item = cJSON_GetObjectItem(root, "indomitable_max"))) pet->indomitable_max = item->valueint;
if ((item = cJSON_GetObjectItem(root, "second_wind_uses"))) pet->second_wind_uses = item->valueint;
if ((item = cJSON_GetObjectItem(root, "superiority_dice"))) pet->superiority_dice = item->valueint;
if ((item = cJSON_GetObjectItem(root, "superiority_dice_max"))) pet->superiority_dice_max = item->valueint;
if ((item = cJSON_GetObjectItem(root, "superiority_dice_size"))) pet->superiority_dice_size = item->valueint;
if ((item = cJSON_GetObjectItem(root, "sneak_attack_dice"))) pet->sneak_attack_dice = item->valueint;
if ((item = cJSON_GetObjectItem(root, "arcane_recovery_used"))) pet->arcane_recovery_used = item->valueint;

cJSON *maneuvers_arr = cJSON_GetObjectItem(root, "maneuvers");
    if (maneuvers_arr && cJSON_IsArray(maneuvers_arr)) {
        cJSON *maneuver_item = NULL;
        cJSON_ArrayForEach(maneuver_item, maneuvers_arr) {
            if (pet->maneuver_count < MAX_MANEUVERS_KNOWN) {
                cJSON *id_item = cJSON_GetObjectItem(maneuver_item, "id");
                if (id_item) {
                    pet->maneuvers[pet->maneuver_count].maneuver_id = (uint8_t)id_item->valueint;
                    pet->maneuver_count++;
                }
            }
        }
    }

if ((item = cJSON_GetObjectItem(root, "ability_points"))) pet->ability_points = item->valueint;
    if ((item = cJSON_GetObjectItem(root, "ability_points_max"))) pet->ability_points_max = item->valueint;

uint32_t saved_salt = 0;
    if ((item = cJSON_GetObjectItem(root, "dna_salt"))) saved_salt = (uint32_t)item->valueint;

    pet->energy_max = MAX_ENERGY;
    pet->is_alive = true;

    cJSON_Delete(root);

    storage_load_dna_codes_only(&pet->dna);

    if (saved_salt != 0) {
        pet->dna.salt = saved_salt;
        ESP_LOGI(TAG, "Using saved salt: 0x%08lX", (unsigned long)pet->dna.salt);
    } else {
        pet->dna.salt = esp_random();
        ESP_LOGI(TAG, "Generated new salt: 0x%08lX", (unsigned long)pet->dna.salt);
    }

    storage_derive_dna_stats(&pet->dna);

    ESP_LOGI(TAG, "Pet loaded: %s Lv.%d (salt=0x%08lX, skills=%d, perks=%d)", 
             pet->name, pet->level, (unsigned long)pet->dna.salt, pet->skill_count, pet->perk_count);

    return true;
}

bool storage_load_tables(uint32_t *exp_table, uint16_t *hp_bonus)
{
    for (int i = 0; i < 100; i++) {
        float linear = 50.0f + (50.0f * (float)(i + 1));
        float exp_part = 50.0f * (powf(1.15f, (float)(i + 1)) - 1.0f);
        exp_table[i] = (uint32_t)(linear + exp_part);
        hp_bonus[i] = 5 + 5 * (i / 2);
    }
    return true;
}

static char *g_professions_json = NULL;
static char *g_enemies_json = NULL;
static char *g_config_json = NULL;

static char* load_json_file(const char *path)
{
    spi_bus_lock();
    FILE *f = fopen(path, "r");
    if (!f) {
        spi_bus_unlock();
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *buffer = malloc(size + 1);
    if (!buffer) {
        fclose(f);
        spi_bus_unlock();
        return NULL;
    }

    size_t read_size = fread(buffer, 1, size, f);
    buffer[read_size] = '\0';
    fclose(f);
    spi_bus_unlock();

    return buffer;
}

bool storage_load_game_tables(void)
{
    if (g_professions_json && g_enemies_json && g_config_json) {
        return true;
    }

    if (!g_professions_json) {
        g_professions_json = load_json_file(MOUNT_POINT "/DATA/TABLES/professions.json");
        if (g_professions_json) {
            ESP_LOGI(TAG, "Professions loaded");
        }
    }

    if (!g_enemies_json) {
        g_enemies_json = load_json_file(MOUNT_POINT "/DATA/TABLES/enemies.json");
        if (g_enemies_json) {
            ESP_LOGI(TAG, "Enemies loaded");
        }
    }

    if (!g_config_json) {
        g_config_json = load_json_file(MOUNT_POINT "/DATA/TABLES/config.json");
        if (g_config_json) {
            ESP_LOGI(TAG, "Config loaded");
        }
    }

    return (g_professions_json != NULL);
}

bool storage_apply_profession_data(pet_t *pet, uint8_t profession_id)
{
    if (!storage_load_game_tables() || !g_professions_json) {
        pet->rest.hp_rest_threshold = 60;
        pet->rest.recovery_chance = 75;
        pet->combat.base_ac = 12;
        pet->combat.damage_dice = 15;
        pet->combat.damage_bonus = 1;
        pet->combat.dice_count = 1;
        pet->hp_max = 20;
        pet->hp = 20;
        pet->energy_max = 10;
        pet->energy = 10;
        return false;
    }

    cJSON *root = cJSON_Parse(g_professions_json);
    if (!root) {
        ESP_LOGE(TAG, "Failed to parse professions.json");
        return false;
    }

    cJSON *professions = cJSON_GetObjectItem(root, "professions");
    if (!professions) {
        cJSON_Delete(root);
        return false;
    }

    cJSON *prof = NULL;
    cJSON_ArrayForEach(prof, professions) {
        cJSON *id_item = cJSON_GetObjectItem(prof, "id");
        if (id_item && id_item->valueint == profession_id) {
            cJSON *threshold = cJSON_GetObjectItem(prof, "hp_rest_threshold");
            cJSON *chance = cJSON_GetObjectItem(prof, "recovery_chance");

            if (threshold) pet->rest.hp_rest_threshold = threshold->valueint;
            else pet->rest.hp_rest_threshold = 60;

            if (chance) pet->rest.recovery_chance = chance->valueint;
            else pet->rest.recovery_chance = 75;

            cJSON *base_ac = cJSON_GetObjectItem(prof, "base_ac");
            cJSON *damage_dice = cJSON_GetObjectItem(prof, "damage_dice");
            cJSON *damage_bonus = cJSON_GetObjectItem(prof, "damage_bonus");
            cJSON *dice_count = cJSON_GetObjectItem(prof, "dice_count");
            cJSON *base_hp = cJSON_GetObjectItem(prof, "base_hp");
            cJSON *base_energy = cJSON_GetObjectItem(prof, "base_energy");

            if (base_ac) pet->combat.base_ac = base_ac->valueint;
            else pet->combat.base_ac = 12;

            if (damage_dice) pet->combat.damage_dice = damage_dice->valueint;
            else pet->combat.damage_dice = 15;

            if (damage_bonus) pet->combat.damage_bonus = damage_bonus->valueint;
            else pet->combat.damage_bonus = 1;

            if (dice_count) pet->combat.dice_count = dice_count->valueint;
            else pet->combat.dice_count = 1;

            if (base_hp) pet->hp_max = base_hp->valueint;
            else pet->hp_max = 20;
            pet->hp = pet->hp_max;

            if (base_energy) pet->energy_max = base_energy->valueint;
            else pet->energy_max = 10;
            pet->energy = pet->energy_max;

            cJSON_Delete(root);
            ESP_LOGI(TAG, "Applied profession %d: HP=%d, EN=%d, AC=%d, dmg=%dd%d+%d",
                     profession_id, pet->hp_max, pet->energy_max,
                     pet->combat.base_ac, pet->combat.dice_count, pet->combat.damage_dice, pet->combat.damage_bonus);
            return true;
        }
    }

    cJSON_Delete(root);
    pet->rest.hp_rest_threshold = 60;
    pet->rest.recovery_chance = 75;
    pet->combat.base_ac = 12;
    pet->combat.damage_dice = 15;
    pet->combat.damage_bonus = 1;
    pet->combat.dice_count = 1;
    pet->hp_max = 20;
    pet->hp = 20;
    pet->energy_max = 10;
    pet->energy = 10;
    return false;
}

bool storage_get_enemy_data(uint8_t enemy_id, uint8_t pet_level, enemy_t *enemy)
{
    if (enemy == NULL) {
        ESP_LOGE(TAG, "storage_get_enemy_data: enemy is NULL");
        return false;
    }

    if (!storage_load_game_tables() || !g_enemies_json) {
        ESP_LOGW(TAG, "storage_get_enemy_data: enemies.json not loaded");
        return false;
    }

    cJSON *root = cJSON_Parse(g_enemies_json);
    if (!root) {
        ESP_LOGE(TAG, "storage_get_enemy_data: JSON parse failed");
        return false;
    }

    cJSON *enemies = cJSON_GetObjectItem(root, "enemies");
    if (!enemies) {
        ESP_LOGE(TAG, "storage_get_enemy_data: 'enemies' array not found");
        cJSON_Delete(root);
        return false;
    }

    cJSON *enemy_data = NULL;
    cJSON_ArrayForEach(enemy_data, enemies) {
        cJSON *id_item = cJSON_GetObjectItem(enemy_data, "id");
        if (id_item && id_item->valueint == enemy_id) {
            cJSON *name = cJSON_GetObjectItem(enemy_data, "name");
            cJSON *base_hp = cJSON_GetObjectItem(enemy_data, "base_hp");
            cJSON *hp_per_level = cJSON_GetObjectItem(enemy_data, "hp_per_level");
            cJSON *base_ac = cJSON_GetObjectItem(enemy_data, "base_ac");
            cJSON *ac_per_level = cJSON_GetObjectItem(enemy_data, "ac_per_level");
cJSON *damage_dice = cJSON_GetObjectItem(enemy_data, "damage_dice");
cJSON *damage_bonus = cJSON_GetObjectItem(enemy_data, "damage_bonus");
cJSON *damage_per_level = cJSON_GetObjectItem(enemy_data, "damage_per_level");
cJSON *attack_bonus = cJSON_GetObjectItem(enemy_data, "attack_bonus");
cJSON *exp_base = cJSON_GetObjectItem(enemy_data, "exp_base");
cJSON *exp_per_level = cJSON_GetObjectItem(enemy_data, "exp_per_level");

if (name) strncpy(enemy->name, name->valuestring, ENEMY_NAME_MAX_LEN - 1);
else snprintf(enemy->name, ENEMY_NAME_MAX_LEN, "Enemy%d", enemy_id);
enemy->name[ENEMY_NAME_MAX_LEN - 1] = '\0';

uint16_t hp_base = base_hp ? base_hp->valueint : 20;
uint16_t hp_pl = hp_per_level ? hp_per_level->valueint : 10;
enemy->hp_max = hp_base + (pet_level * hp_pl);
enemy->hp = enemy->hp_max;

uint8_t ac_base = base_ac ? base_ac->valueint : 10;
uint8_t ac_pl = ac_per_level ? ac_per_level->valueint : 0;
enemy->ac = ac_base + ((pet_level / 5) * ac_pl);
if (enemy->ac > 20) enemy->ac = 20;

enemy->damage_dice = damage_dice ? damage_dice->valueint : 6;
enemy->damage_bonus = damage_bonus ? damage_bonus->valueint : 0;
uint8_t dmg_pl = damage_per_level ? damage_per_level->valueint : 0;
enemy->damage_bonus += (pet_level / 5) * dmg_pl;

enemy->attack_bonus = attack_bonus ? attack_bonus->valueint : 0;

uint16_t avg_damage = (enemy->damage_dice / 2) + 1 + enemy->damage_bonus;
uint16_t exp_from_stats = (enemy->hp_max * enemy->ac * avg_damage) / 20;
uint16_t exp_b = exp_base ? exp_base->valueint : 20;
uint16_t exp_pl = exp_per_level ? exp_per_level->valueint : 5;
enemy->exp_reward = exp_b + (pet_level * exp_pl) + exp_from_stats;

            enemy->level = pet_level;
            enemy->alive = true;

            cJSON_Delete(root);
            ESP_LOGI(TAG, "Enemy %d loaded: %s HP=%d AC=%d dmg=%dd%d+%d exp=%d",
                     enemy_id, enemy->name, enemy->hp_max, enemy->ac,
                     1, enemy->damage_dice, enemy->damage_bonus, enemy->exp_reward);
            return true;
        }
    }

    cJSON_Delete(root);
    return false;
}

uint8_t storage_get_random_enemy_id(uint8_t pet_level)
{
    if (!storage_load_game_tables() || !g_enemies_json) {
        ESP_LOGW(TAG, "get_random_enemy: enemies.json not loaded");
        return 0;
    }

    cJSON *root = cJSON_Parse(g_enemies_json);
    if (!root) {
        ESP_LOGE(TAG, "get_random_enemy: JSON parse failed");
        return 0;
    }

    cJSON *tiers = cJSON_GetObjectItem(root, "enemy_tiers");
    if (!tiers) {
        ESP_LOGE(TAG, "get_random_enemy: 'enemy_tiers' not found");
        cJSON_Delete(root);
        return 0;
    }

    uint8_t valid_tier = 1;
    cJSON *tier = NULL;
    cJSON_ArrayForEach(tier, tiers) {
        cJSON *min_lvl = cJSON_GetObjectItem(tier, "min_level");
        cJSON *max_lvl = cJSON_GetObjectItem(tier, "max_level");
        if (min_lvl && max_lvl) {
            if (pet_level >= min_lvl->valueint && pet_level <= max_lvl->valueint) {
                cJSON *tier_item = cJSON_GetObjectItem(tier, "tier");
                if (tier_item) {
                    valid_tier = tier_item->valueint;
                }
                break;
            }
        }
    }

    ESP_LOGI(TAG, "Pet level %d -> tier %d", pet_level, valid_tier);

    cJSON *enemies = cJSON_GetObjectItem(root, "enemies");
    if (!enemies) {
        ESP_LOGE(TAG, "get_random_enemy: 'enemies' not found");
        cJSON_Delete(root);
        return 0;
    }

    uint8_t candidates[16];
    uint8_t count = 0;

    cJSON *enemy_data = NULL;
    cJSON_ArrayForEach(enemy_data, enemies) {
        cJSON *tier_item = cJSON_GetObjectItem(enemy_data, "tier");
        if (tier_item && tier_item->valueint == valid_tier) {
            cJSON *id_item = cJSON_GetObjectItem(enemy_data, "id");
            if (id_item && count < 16) {
                candidates[count++] = id_item->valueint;
            }
        }
    }

    cJSON_Delete(root);

    ESP_LOGI(TAG, "Found %d enemies for tier %d", count, valid_tier);

    if (count == 0) {
        ESP_LOGW(TAG, "No enemies found for tier %d", valid_tier);
        return 0;
    }

    uint8_t selected = candidates[esp_random() % count];
    ESP_LOGI(TAG, "Selected enemy id: %d", selected);
    return selected;
}

bool storage_file_exists(const char *path)
{
    FILE *f = fopen(path, "r");
    if (f) {
        fclose(f);
        return true;
    }
    return false;
}

esp_err_t storage_save_file(const char *path, const uint8_t *data, size_t len)
{
    char dir_path[128];
    strncpy(dir_path, path, sizeof(dir_path) - 1);

    char *last_slash = strrchr(dir_path, '/');
    if (last_slash) {
        *last_slash = '\0';
        mkdir(dir_path, 0755);
    }

    spi_bus_lock();

    FILE *f = fopen(path, "w");
    if (!f) {
        spi_bus_unlock();
        ESP_LOGE(TAG, "Failed to open %s for writing", path);
        return ESP_FAIL;
    }

    size_t written = fwrite(data, 1, len, f);
    fclose(f);

    spi_bus_unlock();

    if (written != len) {
        ESP_LOGE(TAG, "Write failed: %zu of %zu bytes", written, len);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Saved %s (%zu bytes)", path, len);
    return ESP_OK;
}

esp_err_t storage_delete_file(const char *path)
{
spi_bus_lock();
int result = remove(path);
spi_bus_unlock();

if (result == 0) {
ESP_LOGI(TAG, "Deleted %s", path);
return ESP_OK;
}
return ESP_FAIL;
}

bool storage_load_dna_codes_only(dna_t *dna)
{
    if (dna == NULL) {
        ESP_LOGE(TAG, "storage_load_dna_codes_only: dna is NULL");
        return false;
    }

    spi_bus_lock();

    FILE *f = fopen(MOUNT_POINT "/DATA/DNA/pet_dna.json", "r");
    if (!f) {
        spi_bus_unlock();
        ESP_LOGW(TAG, "DNA file not found, using default codes");
        const char *default_codes[DNA_STAT_COUNT] = {
            "A3fK9x", "B7mP2q", "C1nL8w", "D5hR4t", "E9kM6y", "F2jS7z"
        };
        for (int i = 0; i < DNA_STAT_COUNT; i++) {
            strncpy(dna->codes[i], default_codes[i], DNA_CODE_LEN - 1);
            dna->codes[i][DNA_CODE_LEN - 1] = '\0';
        }
        return false;
    }

    char buf[256];
    size_t len = fread(buf, 1, sizeof(buf) - 1, f);
    buf[len] = '\0';
    fclose(f);

    spi_bus_unlock();

    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        ESP_LOGE(TAG, "storage_load_dna_codes_only: JSON parse failed");
        return false;
    }

    cJSON *item;
    const char *stat_keys[] = {"str", "dex", "con", "int", "wis", "cha"};

    for (int i = 0; i < DNA_STAT_COUNT; i++) {
        item = cJSON_GetObjectItem(root, stat_keys[i]);
        if (item && item->valuestring) {
            strncpy(dna->codes[i], item->valuestring, DNA_CODE_LEN - 1);
            dna->codes[i][DNA_CODE_LEN - 1] = '\0';
        }
    }

    cJSON_Delete(root);

    ESP_LOGI(TAG, "DNA codes loaded: %s %s %s %s %s %s",
             dna->codes[0], dna->codes[1], dna->codes[2],
             dna->codes[3], dna->codes[4], dna->codes[5]);
    return true;
}

void storage_derive_dna_stats(dna_t *dna)
{
    if (dna == NULL) {
        return;
    }

    dna_generate_hash(dna);

    dna_derive_all_stats(dna, dna->base_stats);
    dna_derive_intent_unlock(dna);

    for (uint8_t i = 0; i < DNA_STAT_COUNT; i++) {
        dna->caps[i] = dna->base_stats[i] + DNA_CAP_OFFSET;
    }

    ESP_LOGI(TAG, "DNA stats derived: STR=%d DEX=%d CON=%d INT=%d WIS=%d CHA=%d",
             dna->base_stats[0], dna->base_stats[1], dna->base_stats[2],
             dna->base_stats[3], dna->base_stats[4], dna->base_stats[5]);
}

const char* storage_get_professions_json(void)
{
    return g_professions_json;
}

const char* storage_get_game_tables_json(void)
{
    static char *g_game_tables_json = NULL;
    if (!g_game_tables_json) {
        g_game_tables_json = load_json_file(MOUNT_POINT "/DATA/game_tables.json");
    }
    return g_game_tables_json;
}

static void replay_build_paths(void)
{
    snprintf(g_replay_dir, sizeof(g_replay_dir), REPLAY_BASE_DIR "_a%u", g_replay_num_actions);
    snprintf(g_replay_header_path, sizeof(g_replay_header_path), "%s/header.bin", g_replay_dir);
}

void storage_replay_init(void)
{
    if (!g_mounted) {
        ESP_LOGW(TAG, "Replay init: SD not mounted");
        return;
    }

    if (g_replay_num_actions == 0) {
        g_replay_num_actions = 1;
    }

    spi_bus_lock();

    mkdir(MOUNT_POINT "/BRAIN", 0755);
    mkdir(MOUNT_POINT "/BRAIN/COMBAT", 0755);
    mkdir(REPLAY_BASE_DIR "_a1", 0755);
    mkdir(EPISODES_DIR, 0755);
    mkdir(CHECKPOINTS_DIR, 0755);
    mkdir(EXPLORATION_DIR, 0755);

    replay_build_paths();
    mkdir(g_replay_dir, 0755);

    strncpy(g_replay_data_dir, g_replay_dir, sizeof(g_replay_data_dir) - 1);
    g_replay_data_dir[sizeof(g_replay_data_dir) - 1] = '\0';

    FILE *f = fopen(g_replay_header_path, "rb");
    if (f) {
        fread(&g_replay_header, sizeof(replay_header_t), 1, f);
        fclose(f);
        if (g_replay_header.magic != REPLAY_MAGIC) {
            ESP_LOGW(TAG, "Replay header invalid, recreating");
            memset(&g_replay_header, 0, sizeof(replay_header_t));
        } else if (g_replay_header.num_actions != 0) {
            if (g_replay_header.num_actions != g_replay_num_actions) {
                ESP_LOGI(TAG, "Replay header has a%u, updating from a%u",
                         g_replay_header.num_actions, g_replay_num_actions);
            }
            g_replay_num_actions = g_replay_header.num_actions;
            replay_build_paths();
            strncpy(g_replay_data_dir, g_replay_dir, sizeof(g_replay_data_dir) - 1);
            g_replay_data_dir[sizeof(g_replay_data_dir) - 1] = '\0';
        }
    } else {
        for (uint8_t a = 1; a <= 10; a++) {
            char test_path[80];
            snprintf(test_path, sizeof(test_path), REPLAY_BASE_DIR "_a%u/header.bin", a);
            FILE *tf = fopen(test_path, "rb");
            if (tf) {
                fread(&g_replay_header, sizeof(replay_header_t), 1, tf);
                fclose(tf);
                if (g_replay_header.magic == REPLAY_MAGIC && g_replay_header.num_actions != 0) {
                    g_replay_num_actions = g_replay_header.num_actions;
                    replay_build_paths();
                    strncpy(g_replay_data_dir, g_replay_dir, sizeof(g_replay_data_dir) - 1);
                    g_replay_data_dir[sizeof(g_replay_data_dir) - 1] = '\0';
                    ESP_LOGI(TAG, "Found replay data in a%u (total=%lu)",
                             g_replay_num_actions, (unsigned long)g_replay_header.total_transitions);
                    break;
                }
                memset(&g_replay_header, 0, sizeof(replay_header_t));
            }
        }

        if (g_replay_header.magic != REPLAY_MAGIC) {
            memset(&g_replay_header, 0, sizeof(replay_header_t));
            g_replay_header.magic = REPLAY_MAGIC;
            g_replay_header.version = REPLAY_VERSION;
            g_replay_header.num_actions = g_replay_num_actions;

            FILE *hf = fopen(g_replay_header_path, "wb");
            if (hf) {
                fwrite(&g_replay_header, sizeof(replay_header_t), 1, hf);
                fclose(hf);
                ESP_LOGI(TAG, "Created replay header for a%u", g_replay_num_actions);
            }
        }
    }

    g_replay_initialized = true;

    spi_bus_unlock();

    ESP_LOGI(TAG, "Replay initialized: a%u, %lu chunks, %lu transitions",
             g_replay_num_actions,
             (unsigned long)g_replay_header.total_chunks,
             (unsigned long)g_replay_header.total_transitions);
}

void storage_replay_set_action_count(uint8_t num_actions)
{
    if (num_actions == 0 || num_actions > 6) {
        ESP_LOGE(TAG, "Invalid action count: %u", num_actions);
        return;
    }

    if (num_actions == g_replay_num_actions) return;

    g_replay_num_actions = num_actions;
    replay_build_paths();

    spi_bus_lock();
    mkdir(g_replay_dir, 0755);

    memset(&g_replay_header, 0, sizeof(replay_header_t));
    g_replay_header.magic = REPLAY_MAGIC;
    g_replay_header.version = REPLAY_VERSION;
    g_replay_header.num_actions = num_actions;

    FILE *hf = fopen(g_replay_header_path, "wb");
    if (hf) {
        fwrite(&g_replay_header, sizeof(replay_header_t), 1, hf);
        fclose(hf);
    }

    g_replay_initialized = true;

    spi_bus_unlock();

    ESP_LOGI(TAG, "Replay switched to a%u (new dataset)", num_actions);
}

uint8_t storage_replay_get_action_count(void)
{
    return g_replay_num_actions;
}

void storage_replay_reset(void)
{
    if (!g_mounted) return;

    ESP_LOGI(TAG, "Resetting replay buffer (a%u)...", g_replay_num_actions);

    spi_bus_lock();

    char chunk_path[128];
    for (uint32_t i = 1; i <= g_replay_header.total_chunks; i++) {
        snprintf(chunk_path, sizeof(chunk_path), "%s/chunk_%04lu.bin", g_replay_dir, (unsigned long)i);
        remove(chunk_path);
    }

    remove(g_replay_header_path);

    memset(&g_replay_header, 0, sizeof(replay_header_t));
    g_replay_header.magic = REPLAY_MAGIC;
    g_replay_header.version = REPLAY_VERSION;
    g_replay_header.num_actions = g_replay_num_actions;

    FILE *hf = fopen(g_replay_header_path, "wb");
    if (hf) {
        fwrite(&g_replay_header, sizeof(replay_header_t), 1, hf);
        fclose(hf);
    }

    spi_bus_unlock();

    ESP_LOGI(TAG, "Replay buffer reset complete (a%u)", g_replay_num_actions);
}

void storage_checkpoints_reset(void)
{
    if (!g_mounted) return;

    ESP_LOGI(TAG, "Resetting checkpoints...");

    spi_bus_lock();

    DIR *dir = opendir(CHECKPOINTS_DIR);
    if (dir) {
        struct dirent *ent;
        while ((ent = readdir(dir)) != NULL) {
            if (ent->d_name[0] == '.') continue;
            char filepath[300];
            int len = snprintf(filepath, sizeof(filepath), CHECKPOINTS_DIR "/%s", ent->d_name);
            if (len > 0 && len < (int)sizeof(filepath)) {
                remove(filepath);
            }
        }
        closedir(dir);
    }

    spi_bus_unlock();

    ESP_LOGI(TAG, "Checkpoints reset complete");
}

void storage_replay_append(float *state, uint8_t action, float reward, float *next_state, uint8_t done)
{
    if (g_storage_queue == NULL || !g_replay_initialized) return;

    storage_request_t req;
    req.operation = STORAGE_OP_REPLAY_APPEND;
    memcpy(req.replay.transition.state, state, sizeof(float) * 9);
    req.replay.transition.action = action;
    req.replay.transition.reward = reward;
    memcpy(req.replay.transition.next_state, next_state, sizeof(float) * 9);
    req.replay.transition.done = done;

    xQueueSend(g_storage_queue, &req, 0);
}

bool storage_replay_read_chunk(uint32_t chunk_index, replay_transition_t *out, uint32_t *out_count)
{
    if (!g_mounted || !g_replay_initialized) return false;

    spi_bus_lock();

    char chunk_path[128];
    snprintf(chunk_path, sizeof(chunk_path), "%s/chunk_%04lu.bin", g_replay_data_dir,
             (unsigned long)chunk_index);

    FILE *f = fopen(chunk_path, "rb");
    if (!f) {
        *out_count = 0;
        spi_bus_unlock();
        return false;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    uint32_t count = size / sizeof(replay_transition_t);
    if (out != NULL && count > 0) {
        size_t read = fread(out, sizeof(replay_transition_t), count, f);
        *out_count = read;
    } else {
        *out_count = count;
    }

    fclose(f);
    spi_bus_unlock();

    ESP_LOGI(TAG, "Read chunk %lu: %lu transitions", (unsigned long)chunk_index, (unsigned long)*out_count);
    return true;
}

bool storage_replay_get_stats(replay_stats_t *stats)
{
    if (!g_replay_initialized || stats == NULL) return false;

    stats->total_chunks = g_replay_header.total_chunks;
    stats->total_transitions = g_replay_header.total_transitions;
    stats->current_chunk_transitions = g_replay_header.current_chunk_transitions;
    stats->current_chunk_index = g_replay_header.total_chunks;

    return true;
}

void storage_replay_get_header_path(char *out_path, size_t max_len)
{
    snprintf(out_path, max_len, "%s", g_replay_header_path);
}

void storage_replay_get_dir_path(char *out_path, size_t max_len)
{
    snprintf(out_path, max_len, "%s", g_replay_dir);
}

uint32_t storage_replay_get_total(void)
{
    if (!g_replay_initialized) return 0;
    return g_replay_header.total_transitions;
}

uint32_t storage_replay_calc_transitions_for_level(uint8_t current_level)
{
    return (uint32_t)current_level * 500;
}

bool storage_replay_check_level_up(uint8_t current_level)
{
    uint32_t total = storage_replay_get_total();
    uint32_t required = storage_replay_calc_transitions_for_level(current_level);
    return (total >= required);
}
