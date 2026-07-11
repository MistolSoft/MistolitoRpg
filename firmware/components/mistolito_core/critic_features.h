#ifndef CRITIC_FEATURES_H
#define CRITIC_FEATURES_H

#include <stdint.h>
#include <stdbool.h>
#include <esp_err.h>

#define CRITIC_FEATURE_COUNT    8
#define CRITIC_MAX_HISTORY      5

typedef struct {
    bool hit;
    float p_success;
    float dmg_real;
    float dmg_min;
    float dmg_max;
    float my_hp;
    float enemy_hp;
    float my_damage_taken;
    uint8_t action_taken;
} combat_turn_t;

typedef struct {
    combat_turn_t turns[CRITIC_MAX_HISTORY];
    uint8_t count;
    uint8_t total_turns;
    uint8_t hits;
    float total_damage_dealt;
    float total_damage_taken;
    float my_hp_max;
    float enemy_hp_max;
    float enemy_avg_dmg;
} combat_history_t;

float critic_calc_luck_hit(bool hit, float p_success);
float critic_calc_luck_dmg(float dmg_real, float dmg_min, float dmg_max);
void critic_history_init(combat_history_t *history, float my_hp_max, float enemy_hp_max);
void critic_history_add_turn(combat_history_t *history, const combat_turn_t *turn);
void critic_calc_features(const combat_history_t *history, float features[CRITIC_FEATURE_COUNT]);

#endif
