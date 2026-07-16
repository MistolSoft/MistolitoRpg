#include "value_head.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "spi_bus.h"
#include <string.h>

static const char *TAG = "VALHEAD";

esp_err_t value_head_init(value_head_t *vh)
{
    vh->W = heap_caps_malloc(VALUE_HEAD_FEATURE_SIZE * sizeof(float), MALLOC_CAP_SPIRAM);
    if (!vh->W) {
        return ESP_ERR_NO_MEM;
    }
    memset(vh->W, 0, VALUE_HEAD_FEATURE_SIZE * sizeof(float));
    vh->b = 0.0f;
    vh->loaded = false;
    return ESP_OK;
}

void value_head_deinit(value_head_t *vh)
{
    if (vh->W) {
        heap_caps_free(vh->W);
        vh->W = NULL;
    }
    vh->loaded = false;
}

esp_err_t value_head_forward(const value_head_t *vh, const float *features, float *value_out)
{
    if (!vh->loaded || !vh->W) {
        *value_out = 0.0f;
        return ESP_ERR_INVALID_STATE;
    }
    float sum = vh->b;
    for (int f = 0; f < VALUE_HEAD_FEATURE_SIZE; f++) {
        sum += features[f] * vh->W[f];
    }
    *value_out = sum;
    return ESP_OK;
}

esp_err_t value_head_load(value_head_t *vh, const char *path)
{
    spi_bus_lock();
    FILE *f = fopen(path, "rb");
    if (!f) {
        spi_bus_unlock();
        ESP_LOGW(TAG, "No value head weights found at %s", path);
        return ESP_ERR_NOT_FOUND;
    }

    uint32_t header[3];
    if (fread(header, sizeof(uint32_t), 3, f) != 3) {
        fclose(f);
        spi_bus_unlock();
        return ESP_FAIL;
    }

    if (header[0] != VALUE_HEAD_MAGIC) {
        fclose(f);
        spi_bus_unlock();
        ESP_LOGE(TAG, "Invalid magic number");
        return ESP_ERR_INVALID_VERSION;
    }

    if (fread(vh->W, sizeof(float), VALUE_HEAD_FEATURE_SIZE, f) != VALUE_HEAD_FEATURE_SIZE) {
        fclose(f);
        spi_bus_unlock();
        return ESP_FAIL;
    }

    if (fread(&vh->b, sizeof(float), 1, f) != 1) {
        fclose(f);
        spi_bus_unlock();
        return ESP_FAIL;
    }

    fclose(f);
    spi_bus_unlock();
    vh->loaded = true;
    ESP_LOGI(TAG, "Loaded value head successfully from %s", path);
    return ESP_OK;
}

esp_err_t value_head_save(const value_head_t *vh, uint32_t epoch, const char *path)
{
    if (!vh || !vh->W) {
        return ESP_ERR_INVALID_ARG;
    }

    spi_bus_lock();
    FILE *f = fopen(path, "wb");
    if (!f) {
        spi_bus_unlock();
        ESP_LOGE(TAG, "Failed to open %s for writing", path);
        return ESP_FAIL;
    }

    uint32_t header[3] = {VALUE_HEAD_MAGIC, VALUE_HEAD_VERSION, epoch};
    if (fwrite(header, sizeof(uint32_t), 3, f) != 3) {
        fclose(f);
        spi_bus_unlock();
        return ESP_FAIL;
    }

    if (fwrite(vh->W, sizeof(float), VALUE_HEAD_FEATURE_SIZE, f) != VALUE_HEAD_FEATURE_SIZE) {
        fclose(f);
        spi_bus_unlock();
        return ESP_FAIL;
    }

    if (fwrite(&vh->b, sizeof(float), 1, f) != 1) {
        fclose(f);
        spi_bus_unlock();
        return ESP_FAIL;
    }

    fclose(f);
    spi_bus_unlock();
    ESP_LOGI(TAG, "Saved value head successfully to %s", path);
    return ESP_OK;
}
