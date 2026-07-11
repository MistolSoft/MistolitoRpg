#include "exploration_manager.h"
#include "storage_task.h"
#include "spi_bus.h"
#include "esp_log.h"
#include "esp_random.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "exploration";

static exploration_state_t s_state;
static bool s_initialized = false;

esp_err_t exploration_manager_init(void)
{
    memset(&s_state, 0, sizeof(s_state));
    esp_err_t ret = exploration_manager_load();
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "init: loaded counters from SD");
    } else {
        ESP_LOGI(TAG, "init: no counters found, starting fresh");
    }
    s_initialized = true;
    return ESP_OK;
}

bool exploration_manager_should_force(const pet_t *pet, uint8_t *out_nn_index)
{
    if (!s_initialized || out_nn_index == NULL || pet == NULL) {
        return false;
    }

    for (uint8_t i = 0; i < pet->skill_count; i++) {
        if (s_state.uses_remaining[i] == 0) continue;

        if ((esp_random() % 100) < EXPLORATION_FORCE_PCT) {
            *out_nn_index = 3 + i;
            return true;
        }
    }
    return false;
}

void exploration_manager_on_use(uint8_t skill_slot)
{
    if (!s_initialized || skill_slot >= MAX_SKILLS) return;
    if (s_state.uses_remaining[skill_slot] > 0) {
        s_state.uses_remaining[skill_slot]--;
        s_state.total_forced[skill_slot]++;
        ESP_LOGI(TAG, "on_use: skill_slot %d, remaining=%lu", skill_slot,
                 (unsigned long)s_state.uses_remaining[skill_slot]);
        exploration_manager_save();
    }
}

void exploration_manager_skill_learned(uint8_t skill_slot)
{
    if (!s_initialized || skill_slot >= MAX_SKILLS) return;
    s_state.uses_remaining[skill_slot] = EXPLORATION_FORCED_USES;
    ESP_LOGI(TAG, "skill_learned: slot %d, forced_uses=%d", skill_slot, EXPLORATION_FORCED_USES);
    exploration_manager_save();
}

bool exploration_manager_has_pending(uint8_t skill_count)
{
    if (!s_initialized) return false;
    for (uint8_t i = 0; i < skill_count; i++) {
        if (s_state.uses_remaining[i] > 0) return true;
    }
    return false;
}

esp_err_t exploration_manager_save(void)
{
    esp_err_t ret = storage_save_file(EXPLORATION_FILE,
                                      (const uint8_t *)&s_state,
                                      sizeof(s_state));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "save: failed %s", esp_err_to_name(ret));
    }
    return ret;
}

esp_err_t exploration_manager_load(void)
{
    if (!storage_file_exists(EXPLORATION_FILE)) {
        return ESP_ERR_NOT_FOUND;
    }

    spi_bus_lock();
    FILE *f = fopen(EXPLORATION_FILE, "r");
    if (!f) {
        spi_bus_unlock();
        ESP_LOGE(TAG, "load: open failed");
        return ESP_FAIL;
    }
    size_t read = fread(&s_state, 1, sizeof(s_state), f);
    fclose(f);
    spi_bus_unlock();

    if (read != sizeof(s_state)) {
        memset(&s_state, 0, sizeof(s_state));
        ESP_LOGW(TAG, "load: size mismatch, resetting counters");
        return ESP_ERR_INVALID_SIZE;
    }
    return ESP_OK;
}
