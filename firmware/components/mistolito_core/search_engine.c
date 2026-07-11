#include "search_engine.h"
#include "storage_task.h"
#include "esp_random.h"
#include "esp_log.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static const char *TAG = "SEARCH";

static const uint8_t COMPATIBLE_BIOMES[13][8] = {
    {0, 1, 2, 3, 0, 0, 0, 0},
    {1, 0, 11, 10, 1, 1, 1, 1},
    {2, 0, 6, 4, 2, 2, 2, 2},
    {3, 0, 9, 7, 3, 3, 3, 3},
    {4, 5, 6, 4, 4, 4, 4, 4},
    {5, 4, 11, 10, 5, 5, 5, 5},
    {6, 4, 2, 0, 6, 6, 6, 6},
    {7, 8, 9, 7, 7, 7, 7, 7},
    {8, 7, 12, 10, 8, 8, 8, 8},
    {9, 7, 3, 0, 9, 9, 9, 9},
    {10, 11, 12, 1, 5, 8, 10, 10},
    {11, 10, 1, 5, 11, 11, 11, 11},
    {12, 10, 8, 12, 12, 12, 12, 12}
};

typedef struct {
    uint8_t threshold;
    float reduction_m1;
    float reduction_m2;
} biome_config_t;

static const biome_config_t BIOME_CONFIGS[13] = {
    {45, 0.6f, 0.7f},
    {70, 0.8f, 0.8f},
    {70, 0.8f, 0.8f},
    {70, 0.8f, 0.8f},
    {40, 0.5f, 0.6f},
    {70, 0.8f, 0.8f},
    {70, 0.8f, 0.8f},
    {35, 0.4f, 0.5f},
    {70, 0.8f, 0.8f},
    {70, 0.8f, 0.8f},
    {50, 0.7f, 0.7f},
    {70, 0.8f, 0.8f},
    {70, 0.8f, 0.8f}
};

static const char *get_biome_zone_name(uint8_t biome_id)
{
    if (biome_id <= 3) return "Bosque";
    if (biome_id <= 6) return "Desierto";
    if (biome_id <= 9) return "Nieve";
    return "Agua";
}

static bool is_compatible_with_all_neighbors(uint8_t biome, const uint8_t *neighbor_biomes, uint8_t neighbor_count)
{
    for (uint8_t i = 0; i < neighbor_count; i++) {
        uint8_t nb = neighbor_biomes[i];
        bool found = false;
        for (uint8_t j = 0; j < 8; j++) {
            if (COMPATIBLE_BIOMES[nb][j] == biome) {
                found = true;
                break;
            }
        }
        if (!found) return false;
    }
    return true;
}

static bool is_costa(uint8_t biome)
{
    return (biome == 1 || biome == 5 || biome == 8);
}

static bool is_agua(uint8_t biome)
{
    return (biome >= 10 && biome <= 12);
}

void search_engine_materialize_tile(int16_t x, int16_t y, tile_record_t *tile)
{
    uint8_t neighbor_biomes[8];
    uint8_t neighbor_m1[8];
    uint8_t neighbor_m2[8];
    uint8_t neighbor_m3[8];
    uint8_t neighbor_count = 0;

    for (int8_t dx = -1; dx <= 1; dx++) {
        for (int8_t dy = -1; dy <= 1; dy++) {
            if (dx == 0 && dy == 0) continue;
            tile_record_t nb;
            if (storage_get_tile(x + dx, y + dy, &nb)) {
                neighbor_biomes[neighbor_count] = nb.biome_id;
                neighbor_m1[neighbor_count] = nb.densities[0];
                neighbor_m2[neighbor_count] = nb.densities[1];
                neighbor_m3[neighbor_count] = nb.densities[2];
                neighbor_count++;
            }
        }
    }

    if (neighbor_count == 0) {
        tile->biome_id = esp_random() % 13;
        uint8_t total_target = 10 + (esp_random() % 71);
        uint8_t d1 = (total_target > 0) ? (esp_random() % (total_target + 1)) : 0;
        uint8_t d2 = (total_target > d1) ? (esp_random() % (total_target - d1 + 1)) : 0;
        uint8_t d3 = total_target - d1 - d2;
        tile->densities[0] = d1;
        tile->densities[1] = d2;
        tile->densities[2] = d3;
        storage_set_tile(x, y, tile);
        ESP_LOGI(TAG, "Tile materialized at (%d,%d): biome=%u densities=[%u,%u,%u]", x, y, tile->biome_id, tile->densities[0], tile->densities[1], tile->densities[2]);
        return;
    }

    bool has_agua_neighbor = false;
    for (uint8_t i = 0; i < neighbor_count; i++) {
        if (is_agua(neighbor_biomes[i])) {
            has_agua_neighbor = true;
            break;
        }
    }

    uint8_t compatible_pool[13];
    uint8_t compatible_count = 0;
    for (uint8_t b = 0; b < 13; b++) {
        if (is_costa(b) && !has_agua_neighbor) {
            continue;
        }
        if (is_compatible_with_all_neighbors(b, neighbor_biomes, neighbor_count)) {
            compatible_pool[compatible_count++] = b;
        }
    }

    uint8_t chosen_biome = 0;
    if (compatible_count > 0) {
        if ((esp_random() % 100) < 70) {
            uint8_t chosen_nb = neighbor_biomes[esp_random() % neighbor_count];
            bool in_pool = false;
            for (uint8_t k = 0; k < compatible_count; k++) {
                if (compatible_pool[k] == chosen_nb) {
                    in_pool = true;
                    break;
                }
            }
            if (in_pool) {
                chosen_biome = chosen_nb;
            } else {
                chosen_biome = compatible_pool[esp_random() % compatible_count];
            }
        } else {
            chosen_biome = compatible_pool[esp_random() % compatible_count];
        }
    } else {
        chosen_biome = neighbor_biomes[0];
    }

    tile->biome_id = chosen_biome;

    float avg_m1 = 0;
    float avg_m2 = 0;
    float avg_m3 = 0;
    for (uint8_t i = 0; i < neighbor_count; i++) {
        avg_m1 += neighbor_m1[i];
        avg_m2 += neighbor_m2[i];
        avg_m3 += neighbor_m3[i];
    }
    avg_m1 /= neighbor_count;
    avg_m2 /= neighbor_count;
    avg_m3 /= neighbor_count;

    float m1 = avg_m1;
    float m2 = avg_m2;
    float m3 = avg_m3;
    float low_sum = m1 + m2;
    uint8_t thresh = BIOME_CONFIGS[chosen_biome].threshold;
    if (low_sum > thresh) {
        float old_m1 = m1;
        float old_m2 = m2;
        m1 = old_m1 * BIOME_CONFIGS[chosen_biome].reduction_m1;
        m2 = old_m2 * BIOME_CONFIGS[chosen_biome].reduction_m2;
        m3 = m3 + ((old_m1 - m1) + (old_m2 - m2));
    }

    int16_t delta_m1 = ((int16_t)(esp_random() % 61)) - 30;
    int16_t delta_m2 = ((int16_t)(esp_random() % 61)) - 30;
    int16_t delta_m3 = ((int16_t)(esp_random() % 61)) - 30;

    int16_t f_m1 = (int16_t)m1 + delta_m1;
    int16_t f_m2 = (int16_t)m2 + delta_m2;
    int16_t f_m3 = (int16_t)m3 + delta_m3;

    if (f_m1 < 0) f_m1 = 0; else if (f_m1 > 100) f_m1 = 100;
    if (f_m2 < 0) f_m2 = 0; else if (f_m2 > 100) f_m2 = 100;
    if (f_m3 < 0) f_m3 = 0; else if (f_m3 > 100) f_m3 = 100;

    int32_t total = f_m1 + f_m2 + f_m3;
    if (total > 100) {
        tile->densities[0] = (uint8_t)((f_m1 * 100) / total);
        tile->densities[1] = (uint8_t)((f_m2 * 100) / total);
        tile->densities[2] = 100 - tile->densities[0] - tile->densities[1];
    } else {
        tile->densities[0] = f_m1;
        tile->densities[1] = f_m2;
        tile->densities[2] = f_m3;
    }

    storage_set_tile(x, y, tile);
    ESP_LOGI(TAG, "Tile materialized at (%d,%d): biome=%u densities=[%u,%u,%u]", x, y, tile->biome_id, tile->densities[0], tile->densities[1], tile->densities[2]);
}

void search_engine_init(pet_t *pet)
{
    tile_record_t tile;
    if (!storage_get_tile(pet->world_x, pet->world_y, &tile)) {
        search_engine_materialize_tile(pet->world_x, pet->world_y, &tile);
    }
}

void search_engine_generate_radar(const pet_t *pet, search_radar_t *radar)
{
    uint8_t r = pet->wis / 3 + 1;
    if (r > 4) r = 4;
    radar->radius = r;

    tile_record_t center_tile;
    if (storage_get_tile(pet->world_x, pet->world_y, &center_tile)) {
        radar->current_zone_id = center_tile.biome_id <= 3 ? 0 : (center_tile.biome_id <= 6 ? 1 : (center_tile.biome_id <= 9 ? 2 : 3));
        strncpy(radar->zone_name, get_biome_zone_name(center_tile.biome_id), 15);
        radar->zone_name[15] = '\0';
    } else {
        radar->current_zone_id = 0;
        strcpy(radar->zone_name, "Unknown");
    }

    uint8_t cell_idx = 0;
    int16_t noise_amp = 20 - pet->intel;
    if (noise_amp < 0) noise_amp = 0;

    for (int8_t dx = -((int8_t)r); dx <= (int8_t)r; dx++) {
        for (int8_t dy = -((int8_t)r); dy <= (int8_t)r; dy++) {
            if (cell_idx >= 100) break;

            int16_t gx = pet->world_x + dx;
            int16_t gy = pet->world_y + dy;

            tile_record_t tile;
            if (!storage_get_tile(gx, gy, &tile)) {
                search_engine_materialize_tile(gx, gy, &tile);
            }

            float dist = sqrtf(dx * dx + dy * dy);
            uint8_t base_density = tile.densities[0];

            float noise = 0.0f;
            if (noise_amp > 0) {
                noise = (float)(esp_random() % (noise_amp + 1)) * (dist / 2.0f);
            }

            int16_t perceived = (int16_t)base_density + (int16_t)noise;
            if (perceived > 100) perceived = 100;
            if (perceived < 0) perceived = 0;

            radar->cells[cell_idx].x = gx;
            radar->cells[cell_idx].y = gy;
            radar->cells[cell_idx].density = (uint8_t)perceived;
            radar->cells[cell_idx].detect_dc = 10;
            cell_idx++;
        }
    }
    radar->cell_count = cell_idx;
}

void search_engine_tick(pet_t *pet, search_frame_result_t *result)
{
    if (pet->energy <= 0) {
        result->search_ended = true;
        result->encounter_found = false;
        result->new_data = true;
        return;
    }

    int8_t dx = ((int8_t)(esp_random() % 3)) - 1;
    int8_t dy = ((int8_t)(esp_random() % 3)) - 1;
    if (dx == 0 && dy == 0) {
        dx = 1;
    }

    pet->world_x += dx;
    pet->world_y += dy;

    tile_record_t tile;
    if (!storage_get_tile(pet->world_x, pet->world_y, &tile)) {
        search_engine_materialize_tile(pet->world_x, pet->world_y, &tile);
    }

    int16_t threshold = 90 - (pet->dex * 3);
    if (threshold < 10) threshold = 10;
    if ((int16_t)(esp_random() % 100) < threshold) {
        pet->energy--;
        pet->dirty_flags |= PET_DIRTY_ENERGY;
    }

    uint8_t zone_id = tile.biome_id <= 3 ? 0 : (tile.biome_id <= 6 ? 1 : (tile.biome_id <= 9 ? 2 : 3));
    const char *zone_name = get_biome_zone_name(tile.biome_id);
    ESP_LOGI(TAG, "Pet moved to (%d,%d) | Biome %u (%s) densities=[%u,%u,%u] energy=%u",
             pet->world_x, pet->world_y, tile.biome_id, zone_name, tile.densities[0], tile.densities[1], tile.densities[2], pet->energy);

    uint8_t spawn_roll = esp_random() % 100;
    uint16_t total_spawn_chance = tile.densities[0] + tile.densities[1] + tile.densities[2];

    if (spawn_roll < total_spawn_chance) {
        uint8_t selected_m_idx = 0;
        if (spawn_roll < tile.densities[0]) {
            selected_m_idx = 0;
        } else if (spawn_roll < (uint16_t)tile.densities[0] + tile.densities[1]) {
            selected_m_idx = 1;
        } else {
            selected_m_idx = 2;
        }

        enemy_record_t zone_enemies[8];
        uint8_t enemy_count = storage_get_zone_enemies(zone_id, zone_enemies, 8);

        if (enemy_count > 0) {
            uint8_t target_idx = selected_m_idx;
            if (target_idx >= enemy_count) {
                target_idx = enemy_count - 1;
            }

            enemy_record_t selected_enemy = zone_enemies[target_idx];
            uint8_t roll = (esp_random() % 20) + 1;
            uint8_t check = roll + (pet->wis / 2);

            if (selected_enemy.detect_dc <= check) {
                result->search_ended = true;
                result->encounter_found = true;
                result->detected_enemy_id = selected_enemy.id;
                result->new_data = true;
                ESP_LOGI(TAG, "Enemy detected in %s (ID %d) at (%d,%d) with roll %d + %d",
                         zone_name, selected_enemy.id, pet->world_x, pet->world_y, roll, pet->wis / 2);
                return;
            }
        }
    }

    result->search_ended = false;
    result->encounter_found = false;
    result->new_data = true;

    ESP_LOGI(TAG, "Moved to (%d,%d) in %s, energy remaining: %d",
             pet->world_x, pet->world_y, zone_name, pet->energy);
}
