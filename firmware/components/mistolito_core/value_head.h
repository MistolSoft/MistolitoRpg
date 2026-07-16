#ifndef VALUE_HEAD_H
#define VALUE_HEAD_H

#include <stdint.h>
#include <stdbool.h>
#include <esp_err.h>

#define VALUE_HEAD_FEATURE_SIZE 16
#define VALUE_HEAD_MAGIC 0x43524954
#define VALUE_HEAD_VERSION 1
#define VALUE_HEAD_PATH "/sdcard/BRAIN/COMBAT/value_head.bin"

typedef struct {
    float *W;
    float b;
    bool loaded;
} value_head_t;

esp_err_t value_head_init(value_head_t *vh);
void value_head_deinit(value_head_t *vh);
esp_err_t value_head_forward(const value_head_t *vh, const float *features, float *value_out);
esp_err_t value_head_load(value_head_t *vh, const char *path);
esp_err_t value_head_save(const value_head_t *vh, uint32_t epoch, const char *path);

#endif
