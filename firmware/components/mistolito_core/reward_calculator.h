#ifndef REWARD_CALCULATOR_H
#define REWARD_CALCULATOR_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    int16_t damage_dealt;
    int16_t damage_taken;
    int16_t pet_hp_before;
    int16_t pet_hp_after;
    int16_t pet_hp_max;
    int16_t enemy_hp_before;
    int16_t enemy_hp_after;
    int16_t enemy_hp_max;
    bool enemy_killed;
    bool pet_died;
    bool fled;
    uint8_t action_taken;
} reward_context_t;

#define REWARD_ACTION_ATTACK  0
#define REWARD_ACTION_DEFEND  1
#define REWARD_ACTION_FLEE    2

float reward_calculator_calc(reward_context_t *ctx);

#endif
