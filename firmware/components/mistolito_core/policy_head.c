#include "policy_head.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_random.h"
#include "spi_bus.h"
#include <string.h>
#include <math.h>

static const char *TAG = "POLICY";

static void softmax(const float *input, float *output, int n)
{
    float max_val = input[0];
    for (int i = 1; i < n; i++) {
        if (input[i] > max_val) max_val = input[i];
    }

    float sum = 0.0f;
    for (int i = 0; i < n; i++) {
        output[i] = expf(input[i] - max_val);
        sum += output[i];
    }
    for (int i = 0; i < n; i++) {
        output[i] /= sum;
    }
}

esp_err_t policy_head_init(policy_head_t *ph, uint8_t num_actions)
{
    if (num_actions == 0 || num_actions > POLICY_HEAD_MAX_ACTIONS) {
        return ESP_ERR_INVALID_ARG;
    }

    ph->num_actions = num_actions;
    ph->W = heap_caps_malloc(POLICY_HEAD_FEATURE_SIZE * num_actions * sizeof(float), MALLOC_CAP_SPIRAM);
    ph->b = heap_caps_malloc(num_actions * sizeof(float), MALLOC_CAP_SPIRAM);

    if (!ph->W || !ph->b) {
        heap_caps_free(ph->W);
        heap_caps_free(ph->b);
        return ESP_ERR_NO_MEM;
    }

    memset(ph->W, 0, POLICY_HEAD_FEATURE_SIZE * num_actions * sizeof(float));
    memset(ph->b, 0, num_actions * sizeof(float));

    return ESP_OK;
}

void policy_head_deinit(policy_head_t *ph)
{
    if (ph->W) { heap_caps_free(ph->W); ph->W = NULL; }
    if (ph->b) { heap_caps_free(ph->b); ph->b = NULL; }
    ph->num_actions = 0;
}

esp_err_t policy_head_forward(const policy_head_t *ph, const float *features, float *scores)
{
    if (!ph->W || !ph->b) return ESP_ERR_INVALID_STATE;

    float logits[POLICY_HEAD_MAX_ACTIONS];

    for (int a = 0; a < ph->num_actions; a++) {
        float sum = ph->b[a];
        for (int f = 0; f < POLICY_HEAD_FEATURE_SIZE; f++) {
            sum += features[f] * ph->W[f * ph->num_actions + a];
        }
        logits[a] = sum;
    }

    ESP_LOGD(TAG, "Logits raw: [%.4f, %.4f, %.4f]", logits[0], logits[1], logits[2]);

    softmax(logits, scores, ph->num_actions);

    return ESP_OK;
}

esp_err_t policy_head_expand(policy_head_t *ph, uint8_t new_num_actions)
{
    if (new_num_actions <= ph->num_actions) return ESP_OK;
    if (new_num_actions > POLICY_HEAD_MAX_ACTIONS) return ESP_ERR_INVALID_ARG;

    float *new_W = heap_caps_malloc(POLICY_HEAD_FEATURE_SIZE * new_num_actions * sizeof(float), MALLOC_CAP_SPIRAM);
    float *new_b = heap_caps_malloc(new_num_actions * sizeof(float), MALLOC_CAP_SPIRAM);

    if (!new_W || !new_b) {
        heap_caps_free(new_W);
        heap_caps_free(new_b);
        return ESP_ERR_NO_MEM;
    }

    memset(new_W, 0, POLICY_HEAD_FEATURE_SIZE * new_num_actions * sizeof(float));
    memset(new_b, 0, new_num_actions * sizeof(float));

    for (int f = 0; f < POLICY_HEAD_FEATURE_SIZE; f++) {
        for (int a = 0; a < ph->num_actions; a++) {
            new_W[f * new_num_actions + a] = ph->W[f * ph->num_actions + a];
        }
        for (int a = ph->num_actions; a < new_num_actions; a++) {
            uint32_t r = esp_random();
            new_W[f * new_num_actions + a] = ((float)(r & 0xFFFF) / (float)0xFFFF) * 0.02f - 0.01f;
        }
    }
    memcpy(new_b, ph->b, ph->num_actions * sizeof(float));

    heap_caps_free(ph->W);
    heap_caps_free(ph->b);

    ph->W = new_W;
    ph->b = new_b;
    ph->num_actions = new_num_actions;

    ESP_LOGI(TAG, "Expanded to %d actions", new_num_actions);
    return ESP_OK;
}

esp_err_t policy_head_save(const policy_head_t *ph, uint32_t epoch, const char *path)
{
    spi_bus_lock();
    FILE *f = fopen(path, "wb");
    if (!f) {
        spi_bus_unlock();
        return ESP_FAIL;
    }

    policy_head_meta_t meta = {
        .magic = POLICY_HEAD_MAGIC,
        .version = POLICY_HEAD_VERSION,
        .num_actions = ph->num_actions,
        .epoch = epoch,
        .checksum = 0
    };

    fwrite(&meta, sizeof(meta), 1, f);
    fwrite(ph->W, sizeof(float), POLICY_HEAD_FEATURE_SIZE * ph->num_actions, f);
    fwrite(ph->b, sizeof(float), ph->num_actions, f);
    fclose(f);
    spi_bus_unlock();

    ESP_LOGI(TAG, "Saved policy head (epoch=%lu, actions=%d, path=%s)", (unsigned long)epoch, ph->num_actions, path);
    return ESP_OK;
}

esp_err_t policy_head_load(policy_head_t *ph, const char *path, const char *init_path)
{
    spi_bus_lock();
    FILE *f = fopen(path, "rb");
    if (!f) {
        spi_bus_unlock();
        ESP_LOGW(TAG, "No policy head saved, using init weights");
        return policy_head_load_init(ph, ph->num_actions, init_path);
    }

    policy_head_meta_t meta;
    fread(&meta, sizeof(meta), 1, f);

    if (meta.magic != POLICY_HEAD_MAGIC) {
        fclose(f);
        spi_bus_unlock();
        ESP_LOGW(TAG, "Invalid policy head magic, using init weights");
        return policy_head_load_init(ph, ph->num_actions, init_path);
    }

    uint8_t file_actions = meta.num_actions;
    uint8_t read_actions = (file_actions < ph->num_actions) ? file_actions : ph->num_actions;

    for (int fi = 0; fi < POLICY_HEAD_FEATURE_SIZE; fi++) {
        fread(&ph->W[fi * ph->num_actions], sizeof(float), read_actions, f);
        if (file_actions > read_actions) {
            fseek(f, (file_actions - read_actions) * sizeof(float), SEEK_CUR);
        }
    }
    fread(ph->b, sizeof(float), read_actions, f);
    fclose(f);
    spi_bus_unlock();

    ESP_LOGI(TAG, "Loaded policy head (epoch=%lu, file_actions=%d, active_actions=%d)",
             (unsigned long)meta.epoch, file_actions, ph->num_actions);
    return ESP_OK;
}

esp_err_t policy_head_load_init(policy_head_t *ph, uint8_t num_actions, const char *init_path)
{
    spi_bus_lock();
    FILE *f = fopen(init_path, "rb");
    if (!f) {
        spi_bus_unlock();
        ESP_LOGW(TAG, "No init weights found at %s", init_path);
        return ESP_ERR_NOT_FOUND;
    }

    policy_head_meta_t meta;
    size_t meta_read = fread(&meta, sizeof(meta), 1, f);

    uint8_t file_actions;
    if (meta_read == 1 && meta.magic == POLICY_HEAD_MAGIC) {
        file_actions = meta.num_actions;
        ESP_LOGI(TAG, "Init file: magic OK, file_actions=%d, epoch=%lu", file_actions, (unsigned long)meta.epoch);
    } else {
        fseek(f, 0, SEEK_SET);
        file_actions = num_actions;
        ESP_LOGW(TAG, "Init file: no valid magic, assuming file_actions=%d", file_actions);
    }

    uint8_t read_actions = (file_actions < ph->num_actions) ? file_actions : ph->num_actions;
    ESP_LOGD(TAG, "Loading init weights: file_a=%d, active_a=%d, read_a=%d",
             file_actions, ph->num_actions, read_actions);

    for (int fi = 0; fi < POLICY_HEAD_FEATURE_SIZE; fi++) {
        fread(&ph->W[fi * ph->num_actions], sizeof(float), read_actions, f);
        if (file_actions > read_actions) {
            fseek(f, (file_actions - read_actions) * sizeof(float), SEEK_CUR);
        }
    }
    fread(ph->b, sizeof(float), read_actions, f);
    fclose(f);
    spi_bus_unlock();

    ESP_LOGI(TAG, "Init weights loaded (a=%d): W[0..2]=[%.4f, %.4f, %.4f]",
             read_actions, ph->W[0], ph->W[1], ph->W[2]);
    return ESP_OK;
}
