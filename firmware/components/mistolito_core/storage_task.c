#include "storage_task.h"
#include "game_tables_structs.h"
#include "mistolito.h"
#include "dna_engine.h"
#include "esp_log.h"
#include "esp_random.h"
#include "spi_bus.h"
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
                    FILE *f = fopen(MOUNT_POINT "/BRAIN/PET/pet_data.bin", "wb");
                    if (f) {
                        fwrite(req.pet.pet, sizeof(pet_t), 1, f);
                        fclose(f);
                        ESP_LOGI(TAG, "Pet saved in binary (dirty=0x%04X)", req.pet.dirty_flags);
                    } else {
                        ESP_LOGE(TAG, "Failed to open pet_data.bin for writing");
                    }
                }
                break;

            case STORAGE_OP_LOAD_PET:
                {
                    FILE *f = fopen(MOUNT_POINT "/BRAIN/PET/pet_data.bin", "rb");
                    if (f) {
                        size_t read_bytes = fread(req.pet.pet, 1, sizeof(pet_t), f);
                        fclose(f);
                        if (read_bytes == sizeof(pet_t)) {
                            ESP_LOGI(TAG, "Pet loaded from binary: %s Lv.%d", req.pet.pet->name, req.pet.pet->level);
                        } else {
                            ESP_LOGE(TAG, "Failed to load complete pet binary (%d != %d)", read_bytes, sizeof(pet_t));
                        }
                    } else {
                        ESP_LOGW(TAG, "pet_data.bin not found");
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
    FILE *f = fopen(MOUNT_POINT "/BRAIN/PET/pet_data.bin", "rb");
    if (!f) {
        spi_bus_unlock();
        return false;
    }

    size_t read_bytes = fread(pet, 1, sizeof(pet_t), f);
    fclose(f);
    spi_bus_unlock();

    if (read_bytes != sizeof(pet_t)) {
        ESP_LOGE(TAG, "storage_load_pet: size mismatch (%d != %d)", read_bytes, sizeof(pet_t));
        return false;
    }

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

bool storage_load_game_tables(void)
{
    return storage_file_exists(MOUNT_POINT "/DATA/TABLES/professions.bin");
}

bool storage_apply_profession_data(pet_t *pet, uint8_t profession_id)
{
    spi_bus_lock();
    FILE *f = fopen(MOUNT_POINT "/DATA/TABLES/professions.bin", "rb");
    if (!f) {
        spi_bus_unlock();
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

    profession_record_t prof;
    bool found = false;
    while (fread(&prof, sizeof(profession_record_t), 1, f) == 1) {
        if (prof.id == profession_id) {
            pet->rest.hp_rest_threshold = prof.hp_rest_threshold;
            pet->rest.recovery_chance = prof.recovery_chance;
            pet->combat.base_ac = prof.base_ac;
            pet->combat.damage_dice = prof.damage_dice;
            pet->combat.damage_bonus = prof.damage_bonus;
            pet->combat.dice_count = prof.dice_count;
            pet->hp_max = prof.base_hp;
            pet->hp = pet->hp_max;
            pet->energy_max = prof.base_energy;
            pet->energy = pet->energy_max;
            found = true;
            break;
        }
    }
    fclose(f);
    spi_bus_unlock();

    if (found) {
        ESP_LOGI(TAG, "Applied profession %d: HP=%d, EN=%d, AC=%d, dmg=%dd%d+%d",
                 profession_id, pet->hp_max, pet->energy_max,
                 pet->combat.base_ac, pet->combat.dice_count, pet->combat.damage_dice, pet->combat.damage_bonus);
        return true;
    }

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

    spi_bus_lock();
    FILE *f = fopen(MOUNT_POINT "/DATA/TABLES/enemies.bin", "rb");
    if (!f) {
        spi_bus_unlock();
        ESP_LOGW(TAG, "storage_get_enemy_data: enemies.bin not found");
        return false;
    }

    enemy_record_t rec;
    bool found = false;
    while (fread(&rec, sizeof(enemy_record_t), 1, f) == 1) {
        if (rec.id == enemy_id) {
            strncpy(enemy->name, rec.name, ENEMY_NAME_MAX_LEN - 1);
            enemy->name[ENEMY_NAME_MAX_LEN - 1] = '\0';

            enemy->hp_max = rec.base_hp + (pet_level * rec.hp_per_level);
            enemy->hp = enemy->hp_max;

            enemy->ac = rec.base_ac + ((pet_level / 5) * rec.ac_per_level);
            if (enemy->ac > 20) enemy->ac = 20;

            enemy->damage_dice = rec.damage_dice;
            enemy->damage_bonus = rec.damage_bonus + ((pet_level / 5) * rec.damage_per_level);
            enemy->attack_bonus = rec.attack_bonus;

            uint16_t avg_damage = (enemy->damage_dice / 2) + 1 + enemy->damage_bonus;
            uint16_t exp_from_stats = (enemy->hp_max * enemy->ac * avg_damage) / 20;
            enemy->exp_reward = rec.exp_base + (pet_level * rec.exp_per_level) + exp_from_stats;

            enemy->level = pet_level;
            enemy->alive = true;
            found = true;
            break;
        }
    }
    fclose(f);
    spi_bus_unlock();

    if (found) {
        ESP_LOGI(TAG, "Enemy %d loaded: %s HP=%d AC=%d dmg=%dd%d+%d exp=%d",
                 enemy_id, enemy->name, enemy->hp_max, enemy->ac,
                 1, enemy->damage_dice, enemy->damage_bonus, enemy->exp_reward);
        return true;
    }

    return false;
}

uint8_t storage_get_random_enemy_id(uint8_t pet_level)
{
    spi_bus_lock();
    FILE *f_tier = fopen(MOUNT_POINT "/DATA/TABLES/enemy_tiers.bin", "rb");
    if (!f_tier) {
        spi_bus_unlock();
        ESP_LOGW(TAG, "get_random_enemy: enemy_tiers.bin not found");
        return 0;
    }

    uint8_t valid_tier = 1;
    enemy_tier_record_t tier;
    while (fread(&tier, sizeof(enemy_tier_record_t), 1, f_tier) == 1) {
        if (pet_level >= tier.min_level && pet_level <= tier.max_level) {
            valid_tier = tier.tier;
            break;
        }
    }
    fclose(f_tier);

    ESP_LOGI(TAG, "Pet level %d -> tier %d", pet_level, valid_tier);

    FILE *f_enemy = fopen(MOUNT_POINT "/DATA/TABLES/enemies.bin", "rb");
    if (!f_enemy) {
        spi_bus_unlock();
        ESP_LOGW(TAG, "get_random_enemy: enemies.bin not found");
        return 0;
    }

    uint8_t candidates[16];
    uint8_t count = 0;
    enemy_record_t rec;
    while (fread(&rec, sizeof(enemy_record_t), 1, f_enemy) == 1) {
        if (rec.tier == valid_tier) {
            if (count < 16) {
                candidates[count++] = rec.id;
            }
        }
    }
    fclose(f_enemy);
    spi_bus_unlock();

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
    spi_bus_lock();
    FILE *f = fopen(path, "r");
    if (f) {
        fclose(f);
        spi_bus_unlock();
        return true;
    }
    spi_bus_unlock();
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
    FILE *f = fopen(MOUNT_POINT "/DATA/DNA/pet_dna.bin", "rb");
    if (!f) {
        spi_bus_unlock();
        ESP_LOGW(TAG, "DNA bin file not found, using default codes");
        const char *default_codes[DNA_STAT_COUNT] = {
            "A3fK9x", "B7mP2q", "C1nL8w", "D5hR4t", "E9kM6y", "F2jS7z"
        };
        for (int i = 0; i < DNA_STAT_COUNT; i++) {
            strncpy(dna->codes[i], default_codes[i], DNA_CODE_LEN - 1);
            dna->codes[i][DNA_CODE_LEN - 1] = '\0';
        }
        return false;
    }

    size_t read_bytes = fread(dna->codes, 1, sizeof(dna->codes), f);
    fclose(f);
    spi_bus_unlock();

    if (read_bytes != sizeof(dna->codes)) {
        ESP_LOGE(TAG, "storage_load_dna_codes_only: size mismatch (%d != %d)", read_bytes, sizeof(dna->codes));
        return false;
    }

    for (int i = 0; i < DNA_STAT_COUNT; i++) {
        dna->codes[i][DNA_CODE_LEN - 1] = '\0';
    }

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
