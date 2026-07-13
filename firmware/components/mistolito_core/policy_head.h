#ifndef POLICY_HEAD_H
#define POLICY_HEAD_H

#include <stdint.h>
#include <stdbool.h>
#include <esp_err.h>

#define POLICY_HEAD_MAX_ACTIONS 6
#define POLICY_HEAD_FEATURE_SIZE 16
#define POLICY_HEAD_MAGIC 0x50484544
#define POLICY_HEAD_VERSION 1
typedef struct {
    float *W;
    float *b;
    uint8_t num_actions;
} policy_head_t;

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint8_t num_actions;
    uint32_t epoch;
    uint32_t checksum;
} __attribute__((packed)) policy_head_meta_t;

esp_err_t policy_head_init(policy_head_t *ph, uint8_t num_actions);
void policy_head_deinit(policy_head_t *ph);
esp_err_t policy_head_forward(const policy_head_t *ph, const float *features, float *scores);
esp_err_t policy_head_expand(policy_head_t *ph, uint8_t new_num_actions);
esp_err_t policy_head_save(const policy_head_t *ph, uint32_t epoch, const char *path);
esp_err_t policy_head_load(policy_head_t *ph, const char *path, const char *init_path);
esp_err_t policy_head_load_init(policy_head_t *ph, uint8_t num_actions, const char *init_path);

#endif
