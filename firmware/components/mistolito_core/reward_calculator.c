#include "reward_calculator.h"
#include "esp_log.h"
#include <math.h>

static const char *TAG = "REWARD";

#define REWARD_KILL_BONUS       0.5f
#define REWARD_DEATH_PENALTY   -1.0f
#define REWARD_FLEE_SAFE        0.2f
#define REWARD_FLEE_PENALTY    -0.3f
#define REWARD_HP_DAMAGE_SCALE  0.3f
#define REWARD_DEFEND_BONUS     0.1f

float reward_calculator_calc(reward_context_t *ctx)
{
    if (ctx == NULL) return 0.0f;

    float reward = 0.0f;

    float pet_hp_ratio_before = (ctx->pet_hp_max > 0) ? (float)ctx->pet_hp_before / (float)ctx->pet_hp_max : 0.0f;
    float pet_hp_ratio_after = (ctx->pet_hp_max > 0) ? (float)ctx->pet_hp_after / (float)ctx->pet_hp_max : 0.0f;
    float enemy_hp_ratio_before = (ctx->enemy_hp_max > 0) ? (float)ctx->enemy_hp_before / (float)ctx->enemy_hp_max : 0.0f;
    float enemy_hp_ratio_after = (ctx->enemy_hp_max > 0) ? (float)ctx->enemy_hp_after / (float)ctx->enemy_hp_max : 0.0f;

    float damage_ratio = 0.0f;
    if (ctx->pet_hp_max > 0) {
        damage_ratio = (float)(ctx->damage_dealt - ctx->damage_taken) / (float)ctx->pet_hp_max;
    }
    reward += damage_ratio * REWARD_HP_DAMAGE_SCALE;

    if (ctx->enemy_killed) {
        reward += REWARD_KILL_BONUS;
    }

    if (ctx->pet_died) {
        reward += REWARD_DEATH_PENALTY;
    }

    if (ctx->fled) {
        if (pet_hp_ratio_before < 0.3f) {
            reward += REWARD_FLEE_SAFE;
        } else if (pet_hp_ratio_before > 0.7f) {
            reward += REWARD_FLEE_PENALTY;
        }
    }

    if (ctx->action_taken == REWARD_ACTION_DEFEND && !ctx->fled && !ctx->pet_died) {
        float defend_bonus = REWARD_DEFEND_BONUS * pet_hp_ratio_before;
        reward += defend_bonus;
    }

    if (enemy_hp_ratio_before > 0.5f && enemy_hp_ratio_after < 0.25f) {
        reward += 0.15f;
    }

    if (pet_hp_ratio_before < 0.3f && ctx->damage_dealt > ctx->damage_taken) {
        reward += 0.1f;
    }

    ESP_LOGD(TAG, "R[%s] dealt=%d taken=%d petHP=%.0f%%->%.0f%% enmHP=%.0f%%->%.0f%% => %.4f",
             ctx->action_taken == REWARD_ACTION_ATTACK ? "ATK"
           : ctx->action_taken == REWARD_ACTION_DEFEND ? "DEF"
           : "FLE",
             ctx->damage_dealt, ctx->damage_taken,
             pet_hp_ratio_before * 100.0f, pet_hp_ratio_after * 100.0f,
             enemy_hp_ratio_before * 100.0f, enemy_hp_ratio_after * 100.0f,
             reward);

    return reward;
}

