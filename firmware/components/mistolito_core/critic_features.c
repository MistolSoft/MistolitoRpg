#include "critic_features.h"
#include <string.h>
#include <math.h>

float critic_calc_luck_hit(bool hit, float p_success)
{
    if (hit) {
        return 1.0f - p_success;
    }
    return -p_success;
}

float critic_calc_luck_dmg(float dmg_real, float dmg_min, float dmg_max)
{
    float range = dmg_max - dmg_min;
    if (range <= 0.0f) {
        return 0.0f;
    }
    float normalized = (dmg_real - dmg_min) / range;
    return normalized * 2.0f - 1.0f;
}

void critic_history_init(combat_history_t *history, float my_hp_max, float enemy_hp_max)
{
    memset(history, 0, sizeof(combat_history_t));
    history->my_hp_max = my_hp_max;
    history->enemy_hp_max = enemy_hp_max;
}

void critic_history_add_turn(combat_history_t *history, const combat_turn_t *turn)
{
    if (history->count < CRITIC_MAX_HISTORY) {
        history->turns[history->count] = *turn;
        history->count++;
    } else {
        memmove(&history->turns[0], &history->turns[1],
                (CRITIC_MAX_HISTORY - 1) * sizeof(combat_turn_t));
        history->turns[CRITIC_MAX_HISTORY - 1] = *turn;
    }

    history->total_turns++;
    if (turn->hit) {
        history->hits++;
    }
    history->total_damage_dealt += turn->dmg_real;
    history->total_damage_taken += turn->my_damage_taken;

    if (history->total_turns > 0) {
        history->enemy_avg_dmg = history->total_damage_taken / (float)history->total_turns;
    }
}

void critic_calc_features(const combat_history_t *history, float features[CRITIC_FEATURE_COUNT])
{
    if (history->count == 0) {
        for (int i = 0; i < CRITIC_FEATURE_COUNT; i++) {
            features[i] = 0.0f;
        }
        return;
    }

    float sum_luck_hit = 0.0f;
    float sum_luck_dmg = 0.0f;
    float sum_hp_adv = 0.0f;
    float sum_dmg_momentum = 0.0f;
    float sum_success_rate = 0.0f;
    float sum_threat = 0.0f;
    float sum_turns = 0.0f;
    float sum_win_prob = 0.0f;

    for (uint8_t i = 0; i < history->count; i++) {
        const combat_turn_t *t = &history->turns[i];

        sum_luck_hit += critic_calc_luck_hit(t->hit, t->p_success);
        sum_luck_dmg += critic_calc_luck_dmg(t->dmg_real, t->dmg_min, t->dmg_max);
        sum_hp_adv += (t->my_hp / history->my_hp_max) - (t->enemy_hp / history->enemy_hp_max);

        float my_total = history->total_damage_dealt;
        float en_total = history->total_damage_taken;
        float max_hp = history->my_hp_max > 0 ? history->my_hp_max : 1.0f;
        sum_dmg_momentum += (my_total - en_total) / max_hp;

        if (history->total_turns > 0) {
            sum_success_rate += (float)history->hits / (float)history->total_turns;
        }

        float en_dmg_avg = history->enemy_avg_dmg > 0 ? history->enemy_avg_dmg : 1.0f;
        sum_threat += en_dmg_avg / (t->my_hp > 0 ? t->my_hp : 1.0f);

        float expected = 5.0f;
        sum_turns += (float)history->total_turns / expected;

        float my_hp_ratio = t->my_hp / history->my_hp_max;
        float en_hp_ratio = t->enemy_hp / history->enemy_hp_max;
        float total = my_hp_ratio + en_hp_ratio;
        sum_win_prob += (total > 0) ? my_hp_ratio / total : 0.5f;
    }

    float n = (float)history->count;
    features[0] = sum_luck_hit / n;
    features[1] = sum_luck_dmg / n;
    features[2] = sum_hp_adv / n;
    features[3] = sum_dmg_momentum / n;
    features[4] = sum_success_rate / n;
    features[5] = sum_threat / n;
    features[6] = sum_turns / n;
    features[7] = sum_win_prob / n;

    for (int i = 0; i < CRITIC_FEATURE_COUNT; i++) {
        if (features[i] > 1.0f) features[i] = 1.0f;
        if (features[i] < -1.0f) features[i] = -1.0f;
    }
}
