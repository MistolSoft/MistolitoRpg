#include "dna_engine.h"
#include "storage_task.h"
#include "esp_log.h"
#include "esp_random.h"
#include "psa/crypto.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "DNA_ENGINE";
static bool g_psa_initialized = false;

static esp_err_t ensure_psa_initialized(void)
{
    if (g_psa_initialized) {
        return ESP_OK;
    }
    
    psa_status_t status = psa_crypto_init();
    if (status != PSA_SUCCESS) {
        ESP_LOGE(TAG, "PSA crypto init failed: %d", status);
        return ESP_FAIL;
    }
    
    g_psa_initialized = true;
    ESP_LOGI(TAG, "PSA crypto initialized");
    return ESP_OK;
}

static void sort_ascending(uint8_t *arr, uint8_t len)
{
    for (uint8_t i = 0; i < len - 1; i++) {
        for (uint8_t j = i + 1; j < len; j++) {
            if (arr[j] < arr[i]) {
                uint8_t tmp = arr[i];
                arr[i] = arr[j];
                arr[j] = tmp;
            }
        }
    }
}

static esp_err_t compute_sha256(const uint8_t *input, size_t input_len, uint8_t *output)
{
    if (input == NULL || output == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    esp_err_t ret = ensure_psa_initialized();
    if (ret != ESP_OK) {
        return ret;
    }
    
    size_t out_len;
    psa_status_t status = psa_hash_compute(PSA_ALG_SHA_256, input, input_len, output, DNA_HASH_LEN, &out_len);
    
    if (status != PSA_SUCCESS) {
        ESP_LOGE(TAG, "SHA-256 compute failed: %d", status);
        return ESP_FAIL;
    }
    
    if (out_len != DNA_HASH_LEN) {
        ESP_LOGE(TAG, "SHA-256 output length mismatch: %zu != %d", out_len, DNA_HASH_LEN);
        return ESP_FAIL;
    }
    
    return ESP_OK;
}

esp_err_t dna_init(dna_t *dna)
{
    if (dna == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(dna, 0, sizeof(dna_t));

    const char *default_codes[DNA_STAT_COUNT] = {
        "A3fK9x",
        "B7mP2q",
        "C1nL8w",
        "D5hR4t",
        "E9kM6y",
        "F2jS7z"
    };

    for (uint8_t i = 0; i < DNA_STAT_COUNT; i++) {
        strncpy(dna->codes[i], default_codes[i], DNA_CODE_LEN - 1);
        dna->codes[i][DNA_CODE_LEN - 1] = '\0';
    }

    dna->salt = esp_random();

    dna_generate_hash(dna);

    dna_derive_all_stats(dna, dna->base_stats);
    dna_derive_intent_unlock(dna);

    for (uint8_t i = 0; i < DNA_STAT_COUNT; i++) {
        dna->caps[i] = dna->base_stats[i] + DNA_CAP_OFFSET;
    }

    ESP_LOGI(TAG, "DNA initialized with default codes");
    return ESP_OK;
}

void dna_generate_hash(dna_t *dna)
{
    if (dna == NULL) {
        return;
    }

    uint8_t input_buffer[256];
    size_t offset = 0;

    for (uint8_t i = 0; i < DNA_STAT_COUNT; i++) {
        size_t code_len = strlen(dna->codes[i]);
        if (offset + code_len < sizeof(input_buffer)) {
            memcpy(input_buffer + offset, dna->codes[i], code_len);
            offset += code_len;
        }
    }

    if (offset + sizeof(dna->salt) < sizeof(input_buffer)) {
        memcpy(input_buffer + offset, &dna->salt, sizeof(dna->salt));
        offset += sizeof(dna->salt);
    }

    ESP_LOGI(TAG, "Generating hash from %zu bytes, codes: %s %s %s %s %s %s, salt: 0x%08lX",
             offset, dna->codes[0], dna->codes[1], dna->codes[2],
             dna->codes[3], dna->codes[4], dna->codes[5], (unsigned long)dna->salt);

    esp_err_t ret = compute_sha256(input_buffer, offset, dna->hash);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to generate hash, using fallback");
        for (int i = 0; i < DNA_HASH_LEN; i++) {
            dna->hash[i] = (uint8_t)(esp_random() & 0xFF);
        }
        return;
    }

    ESP_LOGI(TAG, "Hash generated: %02X%02X%02X%02X...",
             dna->hash[0], dna->hash[1], dna->hash[2], dna->hash[3]);
}

uint8_t dna_derive_single_stat(uint8_t *hash, uint8_t stat_idx)
{
    if (hash == NULL || stat_idx >= DNA_STAT_COUNT) {
        return 10;
    }

    uint8_t offset = stat_idx * 5;

    uint8_t d1 = (hash[offset + 0] % 6) + 1;
    uint8_t d2 = (hash[offset + 1] % 6) + 1;
    uint8_t d3 = (hash[offset + 2] % 6) + 1;
    uint8_t d4 = (hash[offset + 3] % 6) + 1;

    uint8_t dice[4] = {d1, d2, d3, d4};
    sort_ascending(dice, 4);

    uint8_t stat = dice[1] + dice[2] + dice[3];

    return stat;
}

void dna_derive_all_stats(dna_t *dna, uint8_t stats[DNA_STAT_COUNT])
{
    if (dna == NULL || stats == NULL) {
        return;
    }

    for (uint8_t i = 0; i < DNA_STAT_COUNT; i++) {
        stats[i] = dna_derive_single_stat(dna->hash, i);
    }

    ESP_LOGI(TAG, "Stats derived: STR=%d DEX=%d CON=%d INT=%d WIS=%d CHA=%d",
             stats[0], stats[1], stats[2], stats[3], stats[4], stats[5]);
}

void dna_derive_intent_unlock(dna_t *dna)
{
    if (dna == NULL) {
        return;
    }

    for (uint8_t i = 0; i < DNA_STAT_COUNT; i++) {
        if (i == 0 || i == 1 || i == 5) {
            dna->intent_unlock[i] = 1;
            continue;
        }

        uint8_t h = dna->hash[i * 4 + 3];
        uint8_t roll = (h % 20) + 1;

        if (roll <= 3) {
            dna->intent_unlock[i] = 0;
        } else if (roll <= 8) {
            dna->intent_unlock[i] = 1;
        } else if (roll <= 13) {
            dna->intent_unlock[i] = 3;
        } else if (roll <= 17) {
            dna->intent_unlock[i] = 5;
        } else {
            dna->intent_unlock[i] = 8;
        }
    }

    ESP_LOGI(TAG, "Intent unlock: ATK=%d DEF=%d HEAL=%d MAGIC=%d SUP=%d FLEE=%d",
             dna->intent_unlock[0], dna->intent_unlock[1], dna->intent_unlock[2],
             dna->intent_unlock[3], dna->intent_unlock[4], dna->intent_unlock[5]);
}

uint8_t dna_roll_d20(dna_t *dna, uint8_t stat_idx, uint32_t action_salt)
{
    if (dna == NULL || stat_idx >= DNA_STAT_COUNT) {
        return (uint8_t)((esp_random() % 20) + 1);
    }
    
    uint8_t input_buffer[128];
    size_t offset = 0;
    
    memcpy(input_buffer + offset, dna->hash, DNA_HASH_LEN);
    offset += DNA_HASH_LEN;
    
    size_t code_len = strlen(dna->codes[stat_idx]);
    if (offset + code_len < sizeof(input_buffer)) {
        memcpy(input_buffer + offset, dna->codes[stat_idx], code_len);
        offset += code_len;
    }
    
    if (offset + sizeof(action_salt) < sizeof(input_buffer)) {
        memcpy(input_buffer + offset, &action_salt, sizeof(action_salt));
        offset += sizeof(action_salt);
    }
    
    uint8_t output[DNA_HASH_LEN];
    esp_err_t ret = compute_sha256(input_buffer, offset, output);
    
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "d20 roll failed, using random");
        return (uint8_t)((esp_random() % 20) + 1);
    }

    uint32_t value = ((uint32_t)output[0] << 24) |
                     ((uint32_t)output[1] << 16) |
                     ((uint32_t)output[2] << 8) |
                     ((uint32_t)output[3]);

    uint8_t roll = (uint8_t)((value % 20) + 1);

    ESP_LOGD(TAG, "d20 roll for stat %d: %d (salt=0x%08lX)", stat_idx, roll, (unsigned long)action_salt);

    return roll;
}

uint8_t dna_roll_d20_context(roll_context_t *ctx)
{
    if (ctx == NULL || ctx->dna == NULL) {
        return (uint8_t)((esp_random() % 20) + 1);
    }

    uint32_t action_salt = (uint32_t)ctx->enemy_id |
                           ((uint32_t)ctx->round << 8) |
                           ((uint32_t)ctx->stat_idx << 16);

    if (ctx->action != NULL) {
        size_t action_len = strlen(ctx->action);
        for (size_t i = 0; i < action_len && i < 4; i++) {
            action_salt |= ((uint32_t)ctx->action[i] << (24 + i * 2));
        }
    }

    return dna_roll_d20(ctx->dna, ctx->stat_idx, action_salt);
}

uint8_t dna_get_cap(dna_t *dna, uint8_t stat_idx)
{
    if (dna == NULL || stat_idx >= DNA_STAT_COUNT) {
        return 25;
    }

    return dna->caps[stat_idx];
}

int8_t dna_get_modifier(uint8_t stat_value)
{
    if (stat_value <= 1) return -5;
    if (stat_value >= 20) return 5;
    return (int8_t)((stat_value / 2) - 5);
}

bool dna_check_stat_increase(dna_t *dna, uint8_t stat_idx, uint8_t current_value, uint8_t level)
{
    if (dna == NULL || stat_idx >= DNA_STAT_COUNT) {
        return false;
    }

    if (current_value >= dna->caps[stat_idx]) {
        ESP_LOGD(TAG, "Stat %d at cap (%d >= %d)", stat_idx, current_value, dna->caps[stat_idx]);
        return false;
    }

    uint8_t dc = 10 + (current_value / 2);

    uint32_t salt = ((uint32_t)level << 8) | (uint32_t)stat_idx;
    uint8_t roll = dna_roll_d20(dna, stat_idx, salt);

    bool success = (roll >= dc);

    ESP_LOGD(TAG, "Stat %d check: roll=%d vs DC=%d → %s",
             stat_idx, roll, dc, success ? "SUCCESS" : "FAIL");

    return success;
}

levelup_queue_t dna_get_levelup_candidates(dna_t *dna, uint8_t current_stats[DNA_STAT_COUNT], uint8_t level)
{
    levelup_queue_t queue;
    memset(&queue, 0, sizeof(levelup_queue_t));

    if (dna == NULL || current_stats == NULL) {
        return queue;
    }

    for (uint8_t i = 0; i < DNA_STAT_COUNT; i++) {
        if (dna_check_stat_increase(dna, i, current_stats[i], level)) {
            queue.candidates[queue.count] = i;
            queue.count++;
        }
    }

    ESP_LOGI(TAG, "Level-up candidates: %d stats can increase", queue.count);

    return queue;
}

void dna_apply_levelup(dna_t *dna, uint8_t stats[DNA_STAT_COUNT], levelup_queue_t *queue)
{
    if (dna == NULL || stats == NULL || queue == NULL) {
        return;
    }

    if (queue->count == 0) {
        ESP_LOGI(TAG, "No stats to increase");
        return;
    }

    uint8_t to_increase = queue->count;
    if (to_increase > 2) {
        uint8_t selected[2];
        uint8_t selected_count = 0;

        for (uint8_t i = 0; i < queue->count && selected_count < 2; i++) {
            if (stats[queue->candidates[i]] < dna->caps[queue->candidates[i]]) {
                selected[selected_count] = queue->candidates[i];
                selected_count++;
            }
        }

        for (uint8_t i = 0; i < selected_count; i++) {
            uint8_t idx = selected[i];
            if (stats[idx] < dna->caps[idx]) {
                stats[idx]++;
                ESP_LOGI(TAG, "Stat %d increased to %d", idx, stats[idx]);
            }
        }
    } else {
        for (uint8_t i = 0; i < queue->count; i++) {
            uint8_t idx = queue->candidates[i];
            if (stats[idx] < dna->caps[idx]) {
                stats[idx]++;
                ESP_LOGI(TAG, "Stat %d increased to %d", idx, stats[idx]);
            }
        }
    }
}

uint8_t dna_engine_get_unlocked_actions(const dna_t *dna, uint8_t level)
{
    if (dna == NULL) return 1;

    uint8_t count = 0;
    for (uint8_t i = 0; i < DNA_STAT_COUNT; i++) {
        if (dna->intent_unlock[i] != 0 && dna->intent_unlock[i] <= level) {
            count++;
        }
    }
    return count > 0 ? count : 1;
}
