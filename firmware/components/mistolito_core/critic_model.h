#ifndef CRITIC_MODEL_H
#define CRITIC_MODEL_H

#include <stdint.h>
#include <stdbool.h>
#include <esp_err.h>
#include "critic_features.h"

#define CRITIC_MODEL_MAGIC      0x43524954
#define CRITIC_MODEL_VERSION    1
#define CRITIC_HIDDEN_SIZE      4
#define CRITIC_MODEL_PATH       "/sdcard/BRAIN/COMBAT/critic.bin"

typedef struct {
    float W1[CRITIC_FEATURE_COUNT * CRITIC_HIDDEN_SIZE];
    float b1[CRITIC_HIDDEN_SIZE];
    float W2[CRITIC_HIDDEN_SIZE];
    float b2;
} critic_model_weights_t;

typedef struct {
    critic_model_weights_t weights;
    bool loaded;
} critic_model_t;

esp_err_t critic_model_load(critic_model_t *model);
esp_err_t critic_model_forward(critic_model_t *model, const float features[CRITIC_FEATURE_COUNT], float *quality_score);
float critic_model_get_last_score(const critic_model_t *model);

#endif
