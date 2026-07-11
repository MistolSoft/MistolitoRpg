#include "critic_model.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

static const char *TAG = "CRITIC";

static float relu(float x)
{
    return x > 0.0f ? x : 0.0f;
}

static float tanh_activation(float x)
{
    float e2x = expf(2.0f * x);
    return (e2x - 1.0f) / (e2x + 1.0f);
}

esp_err_t critic_model_load(critic_model_t *model)
{
    FILE *f = fopen(CRITIC_MODEL_PATH, "rb");
    if (!f) {
        ESP_LOGW(TAG, "Critic model not found at %s", CRITIC_MODEL_PATH);
        model->loaded = false;
        return ESP_ERR_NOT_FOUND;
    }

    size_t read = fread(&model->weights, sizeof(critic_model_weights_t), 1, f);
    fclose(f);

    if (read != 1) {
        ESP_LOGE(TAG, "Failed to read critic model");
        model->loaded = false;
        return ESP_FAIL;
    }

    model->loaded = true;
    ESP_LOGI(TAG, "Critic model loaded from SD");
    return ESP_OK;
}

esp_err_t critic_model_forward(critic_model_t *model, const float features[CRITIC_FEATURE_COUNT], float *quality_score)
{
    if (!model->loaded) {
        *quality_score = 0.0f;
        return ESP_ERR_INVALID_STATE;
    }

    float hidden[CRITIC_HIDDEN_SIZE];

    for (int h = 0; h < CRITIC_HIDDEN_SIZE; h++) {
        float sum = model->weights.b1[h];
        for (int i = 0; i < CRITIC_FEATURE_COUNT; i++) {
            sum += features[i] * model->weights.W1[i * CRITIC_HIDDEN_SIZE + h];
        }
        hidden[h] = relu(sum);
    }

    float output = model->weights.b2;
    for (int h = 0; h < CRITIC_HIDDEN_SIZE; h++) {
        output += hidden[h] * model->weights.W2[h];
    }

    *quality_score = tanh_activation(output);

    return ESP_OK;
}

float critic_model_get_last_score(const critic_model_t *model)
{
    (void)model;
    return 0.0f;
}
