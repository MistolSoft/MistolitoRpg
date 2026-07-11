#ifndef PPO_TRAINER_H
#define PPO_TRAINER_H

#include <stdint.h>
#include <stdbool.h>
#include <esp_err.h>
#include "policy_head.h"
#include "training_mode.h"

#define PPO_INPUT_SIZE      9
#define PPO_STATE_SIZE      16
#define PPO_MAX_ACTIONS     6

#define PPO_CHECKPOINT_MAGIC 0x50504F43
#define PPO_CHECKPOINT_VERSION 3

#define PPO_CHECKPOINT_DIR          CHECKPOINTS_DIR
#define PPO_CHECKPOINT_POLICY_W     PPO_CHECKPOINT_DIR "/policy_w_%03d.bin"
#define PPO_CHECKPOINT_POLICY_B     PPO_CHECKPOINT_DIR "/policy_b_%03d.bin"
#define PPO_CHECKPOINT_META         PPO_CHECKPOINT_DIR "/checkpoint_%03d.meta"

typedef struct {
    float lr;
    float gamma;
    float lambda;
    float clip_coef;
    float ent_coef;
    float vf_coef;
    float max_grad_norm;
    float momentum;
    int epochs;
    int batch_size;
} ppo_config_t;

typedef struct {
    float state[PPO_STATE_SIZE];
    uint8_t action;
    float reward;
    float next_state[PPO_STATE_SIZE];
    uint8_t done;
    float old_log_prob;
    float advantage;
    float return_value;
} ppo_trajectory_t;

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t epoch;
    float loss;
    uint32_t timestamp;
    uint32_t checksum;
    uint8_t num_actions;
    uint8_t reserved[3];
} __attribute__((packed)) ppo_checkpoint_meta_t;

esp_err_t ppo_train_policy(policy_head_t *ph, const ppo_config_t *config,
                           training_progress_t *progress);

esp_err_t ppo_save_checkpoint(policy_head_t *ph, uint32_t epoch, float loss);
esp_err_t ppo_load_checkpoint(policy_head_t *ph, uint32_t epoch);
bool ppo_find_latest_checkpoint(uint32_t *epoch_out);

ppo_config_t ppo_config_default(void);

#endif
