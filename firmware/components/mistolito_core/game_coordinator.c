#include "cJSON.h"
#include "game_tables_structs.h"

#include "game_coordinator.h"
#include "combat_engine.h"
#include "rest_engine.h"
#include "events.h"
#include "storage_task.h"
#include "display_task.h"
#include "rules.h"
#include "dna_engine.h"
#include "profession_engine.h"
#include "skills_perks_engine.h"
#include "resources_engine.h"
#include "features_engine.h"
#include "spell_engine.h"
#include "inference_engine.h"
#include "reward_calculator.h"
#include "experience_logger.h"
#include "exploration_manager.h"
#include "skill_resolver.h"
#include "ppo_trainer.h"
#include "training_mode.h"
#include "critic_features.h"
#include "critic_model.h"
#include "policy_head.h"
#include "rules.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_random.h"
#include "freertos/semphr.h"
#include <string.h>
#include <math.h>

static const char *TAG = "COORD";

static game_snapshot_t g_snapshot;
static bool g_initialized = false;
static SemaphoreHandle_t g_snapshot_mutex = NULL;

static combat_frame_result_t g_combat_result;
static uint8_t g_resolved_skill_id = 0xFF;
static bool g_combat_active = false;

static search_frame_result_t g_search_result;
static uint32_t g_search_start_ms = 0;
static uint32_t g_search_duration_ms = 0;

static rest_frame_result_t g_rest_result;

static critic_model_t g_critic_model;
static combat_history_t g_combat_history;
static float g_last_quality_score = 0.0f;
static policy_head_t g_policy_head;

typedef struct {
    char enemy_name[ENEMY_NAME_MAX_LEN];
    int16_t enemy_hp_start;
    int16_t enemy_hp_max;
    int16_t pet_hp_start;
    uint16_t turn_count;
    uint16_t attack_count;
    uint16_t defend_count;
    uint16_t flee_count;
    uint16_t flee_success;
    uint16_t total_damage_dealt;
    uint16_t total_damage_taken;
} combat_log_t;
static combat_log_t g_combat_log;

TaskHandle_t g_coordinator_task_handle = NULL;

static float exp_base = 50.0f;
static float exp_linear = 50.0f;
static float exp_multiplier = 1.15f;

typedef struct {
    uint32_t max_level;
    uint32_t transitions_per_level;
} transition_interval_t;

static transition_interval_t s_intervals[10];
static int s_intervals_count = 0;

static void load_transition_intervals(void)
{
    FILE *f = fopen(MOUNT_POINT "/DATA/TABLES/transition_intervals.bin", "rb");
    if (!f) {
        ESP_LOGW(TAG, "transition_intervals.bin not found");
        return;
    }

    s_intervals_count = 0;
    while (s_intervals_count < 10 && fread(&s_intervals[s_intervals_count], sizeof(transition_interval_t), 1, f) == 1) {
        s_intervals_count++;
    }
    fclose(f);
}

static bool check_level_up_trigger(uint8_t current_level, uint32_t total_trans)
{
    uint32_t transitions_per_level = 500;
    if (s_intervals_count > 0) {
        for (int i = 0; i < s_intervals_count; i++) {
            if (current_level <= s_intervals[i].max_level) {
                transitions_per_level = s_intervals[i].transitions_per_level;
                break;
            }
        }
    }

    if (transitions_per_level == 0) {
        return false;
    }

    uint32_t calculated_level = total_trans / transitions_per_level;
    return (calculated_level >= current_level);
}

static uint32_t calc_exp_for_level(uint8_t level)
{
    float linear_part = exp_base + (exp_linear * (float)level);
    float exp_part = exp_base * (powf(exp_multiplier, (float)level) - 1.0f);
    return (uint32_t)(linear_part + exp_part);
}

static uint16_t calc_hp_bonus(uint8_t level)
{
    return 5 + 5 * ((level - 1) / 2);
}

static void transition_to(game_state_e new_state)
{
    g_snapshot.state = new_state;
    g_snapshot.state_entered_ms = (uint32_t)(esp_timer_get_time() / 1000);

    game_event_t evt = { .type = EVT_STATE_CHANGED, .data.new_state = new_state };
    events_send(&evt);

    ESP_LOGI(TAG, "State transition: %d", new_state);
}

static void emit_level_up_event(uint8_t level)
{
    game_event_t evt = { .type = EVT_LEVEL_UP, .data.level_up.level = level };
    events_send(&evt);
}

static void emit_enemy_spawned_event(void)
{
    game_event_t evt = { .type = EVT_ENEMY_SPAWNED };
    events_send(&evt);
}

void game_coordinator_start(void)
{
    events_init();

    if (g_snapshot_mutex == NULL) {
        g_snapshot_mutex = xSemaphoreCreateMutex();
    }

    inference_engine_init();
    exploration_manager_init();

    critic_model_load(&g_critic_model);
    critic_history_init(&g_combat_history, 1.0f, 1.0f);

    policy_head_init(&g_policy_head, 6);
    policy_head_load(&g_policy_head);

    memset(&g_snapshot, 0, sizeof(g_snapshot));
    memset(&g_combat_result, 0, sizeof(g_combat_result));
    g_combat_active = false;

    strncpy(g_snapshot.pet.name, "Mistolito", PET_NAME_MAX_LEN);
    g_snapshot.pet.level = 1;
    g_snapshot.pet.profession = PROF_NONE;

    storage_load_dna_codes_only(&g_snapshot.pet.dna);

    g_snapshot.state = GS_INIT;
    g_snapshot.state_entered_ms = (uint32_t)(esp_timer_get_time() / 1000);
    g_initialized = true;

    ESP_LOGI(TAG, "Coordinator started (INIT mode)");
}

game_snapshot_t* game_coordinator_get_snapshot(void)
{
    return &g_snapshot;
}

SemaphoreHandle_t game_coordinator_get_mutex(void)
{
    return g_snapshot_mutex;
}

QueueHandle_t game_coordinator_get_turn_done_queue(void)
{
    return NULL;
}

combat_frame_result_t* game_coordinator_get_combat_result(void)
{
    return &g_combat_result;
}

void game_coordinator_clear_combat_result(void)
{
    g_combat_result.pet_damage_this_frame = 0;
    g_combat_result.enemy_damage_this_frame = 0;
    g_combat_result.pet_hit_this_frame = false;
    g_combat_result.enemy_hit_this_frame = false;
    g_combat_result.turns_this_frame = 0;
    g_combat_result.new_data = false;
}

search_frame_result_t* game_coordinator_get_search_result(void)
{
    return &g_search_result;
}

void game_coordinator_clear_search_result(void)
{
    g_search_result.search_ended = false;
    g_search_result.encounter_found = false;
    g_search_result.new_data = false;
}

rest_frame_result_t* game_coordinator_get_rest_result(void)
{
    return &g_rest_result;
}

void game_coordinator_clear_rest_result(void)
{
    g_rest_result.hp_recovered = 0;
    g_rest_result.energy_recovered = 0;
    g_rest_result.rest_ended = false;
    g_rest_result.new_data = false;
}

static void process_level_up(void)
{
uint8_t new_level = g_snapshot.pet.level + 1;
g_snapshot.pet.level = new_level;
g_snapshot.pet.exp_next = calc_exp_for_level(new_level);

uint16_t hp_bonus = calc_hp_bonus(new_level);
g_snapshot.pet.hp_max += hp_bonus;
g_snapshot.pet.hp = g_snapshot.pet.hp_max;
g_snapshot.pet.energy = g_snapshot.pet.energy_max;

int8_t con_mod = dna_get_modifier(g_snapshot.pet.con);
if (con_mod > 0) {
g_snapshot.pet.hp_max += con_mod;
}

profession_increment_level(&g_snapshot.pet);

uint8_t current_stats[DNA_STAT_COUNT] = {
g_snapshot.pet.str, g_snapshot.pet.dex, g_snapshot.pet.con,
g_snapshot.pet.intel, g_snapshot.pet.wis, g_snapshot.pet.cha
};

levelup_queue_t queue = dna_get_levelup_candidates(&g_snapshot.pet.dna, current_stats, new_level);
dna_apply_levelup(&g_snapshot.pet.dna, current_stats, &queue);

g_snapshot.pet.str = current_stats[STAT_STR];
g_snapshot.pet.dex = current_stats[STAT_DEX];
g_snapshot.pet.con = current_stats[STAT_CON];
g_snapshot.pet.intel = current_stats[STAT_INT];
g_snapshot.pet.wis = current_stats[STAT_WIS];
g_snapshot.pet.cha = current_stats[STAT_CHA];

if (g_snapshot.pet.profession == PROF_NONE) {
uint8_t available_profs[PROF_COUNT];
uint8_t prof_count = 0;

for (uint8_t prof_id = 1; prof_id < PROF_COUNT; prof_id++) {
if (profession_check_requirements(&g_snapshot.pet, prof_id)) {
available_profs[prof_count++] = prof_id;
}
}

if (prof_count > 0) {
uint8_t chosen_idx = (prof_count > 1) ? (esp_random() % prof_count) : 0;
if (profession_try_change(&g_snapshot.pet, available_profs[chosen_idx])) {
resources_apply_profession(&g_snapshot.pet);
}
}
}

if (g_snapshot.pet.profession != PROF_NONE) {
profession_apply_level_bonuses(&g_snapshot.pet, g_snapshot.pet.profession_level);

profession_apply_combat_stats(&g_snapshot.pet);

resources_apply_level(&g_snapshot.pet, g_snapshot.pet.profession_level);

feature_candidate_t features[MAX_FEATURE_CANDIDATES];
uint8_t feature_count = features_get_available(&g_snapshot.pet, g_snapshot.pet.profession_level, features, MAX_FEATURE_CANDIDATES);
for (uint8_t i = 0; i < feature_count; i++) {
features_try_learn(&g_snapshot.pet, &features[i]);
}

if (g_snapshot.pet.profession == PROF_MAGE) {
spell_candidate_t spells[MAX_SPELL_CANDIDATES];
uint8_t spell_count = spells_get_available(&g_snapshot.pet, spells, MAX_SPELL_CANDIDATES);
for (uint8_t i = 0; i < spell_count; i++) {
spells_try_learn(&g_snapshot.pet, &spells[i]);
}
}

skills_perks_process_level_up(&g_snapshot.pet, g_snapshot.pet.profession_level);

uint8_t bonus_stats[STAT_COUNT];
uint8_t bonus_count = 0;
profession_get_bonus_stats(&g_snapshot.pet, bonus_stats, &bonus_count);

for (uint8_t i = 0; i < bonus_count; i++) {
uint8_t stat_idx = bonus_stats[i];
if (profession_try_stat_increase(&g_snapshot.pet, stat_idx, true)) {
switch (stat_idx) {
case STAT_STR: g_snapshot.pet.str++; break;
case STAT_DEX: g_snapshot.pet.dex++; break;
case STAT_CON: g_snapshot.pet.con++; break;
case STAT_INT: g_snapshot.pet.intel++; break;
case STAT_WIS: g_snapshot.pet.wis++; break;
case STAT_CHA: g_snapshot.pet.cha++; break;
}
}
}
}

for (uint8_t i = 0; i < DNA_STAT_COUNT; i++) {
    if (g_snapshot.pet.dna.intent_unlock[i] == new_level) {
        exploration_manager_unlock(i);
        ESP_LOGI(TAG, "Intent %d unlocked at level %d", i, new_level);
    }
}

g_snapshot.pet.dirty_flags |= PET_DIRTY_LEVEL | PET_DIRTY_HP | PET_DIRTY_STATS | PET_DIRTY_PROF_LEVEL | PET_DIRTY_SKILLS | PET_DIRTY_PERKS | PET_DIRTY_SPELLS | PET_DIRTY_RESOURCES | PET_DIRTY_MANEUVERS;

emit_level_up_event(new_level);
storage_save_pet_delta(g_snapshot.pet.dirty_flags, &g_snapshot.pet);
g_snapshot.pet.dirty_flags = 0;

ESP_LOGI(TAG, "LEVEL UP! Now level %d", new_level);
}

static void try_second_wind(void)
{
if (g_snapshot.pet.profession != PROF_WARRIOR) return;
if (g_snapshot.pet.second_wind_uses == 0) return;
if (g_snapshot.pet.hp >= g_snapshot.pet.hp_max / 2) return;
if ((esp_random() % 100) >= 50) return;

g_snapshot.pet.second_wind_uses--;
uint8_t heal = rules_roll_dice(10) + g_snapshot.pet.profession_level;
g_snapshot.pet.hp += heal;
if (g_snapshot.pet.hp > g_snapshot.pet.hp_max) g_snapshot.pet.hp = g_snapshot.pet.hp_max;

ESP_LOGI(TAG, "Second Wind: healed %d HP", heal);
}

static void calc_combat_inputs(pet_t *pet, enemy_t *enemy, float inputs[9], float quality_score)
{
    int8_t pet_dex_mod = rules_get_modifier(pet->dex);
    int8_t pet_str_mod = rules_get_modifier(pet->str);
    int16_t pet_ac = pet->combat.base_ac + pet_dex_mod;

    int16_t roll_needed_hit = enemy->ac - pet_dex_mod;
    if (roll_needed_hit < 2) roll_needed_hit = 2;
    if (roll_needed_hit > 20) roll_needed_hit = 20;
    float hit_prob = (21.0f - (float)roll_needed_hit) / 20.0f;

    int16_t roll_needed_def = pet_ac - enemy->attack_bonus;
    if (roll_needed_def < 2) roll_needed_def = 2;
    if (roll_needed_def > 20) roll_needed_def = 20;
    float defense_prob = (21.0f - (float)roll_needed_def) / 20.0f;

    float enemy_hp_ratio = (enemy->hp_max > 0) ? (float)enemy->hp / (float)pet->hp : 1.0f;
    float level_ratio = (pet->level > 0) ? (float)enemy->level / (float)pet->level : 1.0f;

    uint8_t dice_count = 3 + pet->bonuses.extra_dice;
    float avg_pet_dmg = 0.0f;
    switch (pet->profession) {
        case 0:
            avg_pet_dmg = dice_count * 3.5f + pet_str_mod + pet->bonuses.min_damage;
            break;
        case 1:
            avg_pet_dmg = (1 + pet->bonuses.extra_dice) * 10.5f + pet_str_mod + pet->bonuses.min_damage;
            break;
        case 2:
            avg_pet_dmg = dice_count * 4.0f + pet_str_mod + pet->bonuses.min_damage;
            break;
        default:
            avg_pet_dmg = pet->combat.dice_count * ((pet->combat.damage_dice + 1) / 2.0f) + pet->combat.damage_bonus + pet_str_mod + pet->bonuses.min_damage;
            break;
    }
    float avg_enemy_dmg = ((enemy->damage_dice + 1) / 2.0f) + enemy->damage_bonus;

    float dmg_efficiency = (enemy->hp > 0) ? avg_pet_dmg / (float)enemy->hp : 1.0f;
    float threat_level = (pet->hp > 0) ? avg_enemy_dmg / (float)pet->hp : 1.0f;

    inputs[0] = (float)pet->hp / pet->hp_max;
    inputs[1] = (float)pet->energy / pet->energy_max;
    inputs[2] = hit_prob;
    inputs[3] = defense_prob;
    inputs[4] = enemy_hp_ratio;
    inputs[5] = level_ratio;
    inputs[6] = dmg_efficiency;
    inputs[7] = threat_level;
    inputs[8] = quality_score;
}

static combat_action_e decide_combat_action(pet_t *pet, enemy_t *enemy)
{
    uint8_t forced_intention;
    if (exploration_manager_should_force(pet->level, &pet->dna, &forced_intention)) {
        exploration_manager_on_use(forced_intention);
        uint8_t skill_id = skill_resolver_resolve(pet, forced_intention);
        if (skill_id != 0xFF) {
            g_resolved_skill_id = skill_id;
            ESP_LOGI(TAG, "Forced intention %d -> skill %d", forced_intention, skill_id);
            return ACTION_SKILL;
        }
        ESP_LOGW(TAG, "Forced intention %d but no skill, falling through", forced_intention);
    }

    uint8_t num_actions = dna_engine_get_unlocked_actions(&g_snapshot.pet.dna, pet->level);

    float input[9];

    calc_combat_inputs(pet, enemy, input, g_last_quality_score);

    if (inference_engine_is_loaded()) {
        float features[16];
        esp_err_t ret = inference_engine_run(input, 9, features, 16);
        if (ret == ESP_OK) {
            float scores[PPO_MAX_ACTIONS];
            ret = policy_head_forward(&g_policy_head, features, scores);
            if (ret == ESP_OK) {
                uint8_t best_idx = 0;
                float best_score = scores[0];
                for (uint8_t i = 1; i < num_actions; i++) {
                    if (scores[i] > best_score) {
                        best_score = scores[i];
                        best_idx = i;
                    }
                }

                uint8_t skill_id = skill_resolver_resolve(pet, best_idx);
                if (skill_id != 0xFF) {
                    g_resolved_skill_id = skill_id;
                    ESP_LOGI(TAG, "Inference: intention %d -> skill %d (score=%.3f)", best_idx, skill_id, best_score);
                    return ACTION_SKILL;
                }
                ESP_LOGW(TAG, "Inference: intention %d has no skill, trying next", best_idx);
            } else {
                ESP_LOGW(TAG, "Policy head failed: %s", esp_err_to_name(ret));
            }
        } else {
            ESP_LOGW(TAG, "Inference run failed: %s", esp_err_to_name(ret));
        }
    }

    ESP_LOGD(TAG, "Fallback ATACAR (model not loaded or no skill)");
    return ACTION_ATTACK;
}

static void simulate_combat_turn(void)
{
    if (!g_snapshot.pet.is_alive || combat_engine_all_enemies_dead(&g_snapshot.encounter)) {
        ESP_LOGV(TAG, "simulate_combat_turn skipped: alive=%d, enemies_dead=%d",
                 g_snapshot.pet.is_alive, combat_engine_all_enemies_dead(&g_snapshot.encounter));
        return;
    }

    uint8_t target_idx = combat_engine_select_first_alive(&g_snapshot.encounter);
    enemy_t *target = &g_snapshot.encounter.enemies[target_idx];

    g_snapshot.combat.defend_bonus_ac = 0;

    g_combat_result.pet_hit_this_frame = false;
    g_combat_result.enemy_hit_this_frame = false;
    g_combat_result.pet_damage_this_frame = 0;
    g_combat_result.enemy_damage_this_frame = 0;

    g_combat_log.turn_count++;

    if (g_combat_log.turn_count == 1) {
        strncpy(g_combat_log.enemy_name, target->name, ENEMY_NAME_MAX_LEN - 1);
        g_combat_log.enemy_name[ENEMY_NAME_MAX_LEN - 1] = '\0';
        g_combat_log.enemy_hp_start = target->hp;
        g_combat_log.enemy_hp_max = target->hp_max;
        g_combat_log.pet_hp_start = g_snapshot.pet.hp;
    }

    int16_t pet_hp_before = g_snapshot.pet.hp;
    int16_t enemy_hp_before = target->hp;

    float state_before[9];
    calc_combat_inputs(&g_snapshot.pet, target, state_before, g_last_quality_score);

    combat_action_e action = decide_combat_action(&g_snapshot.pet, target);

    uint8_t action_idx = 0;
    if (action == ACTION_DEFEND) action_idx = 1;
    else if (action == ACTION_FLEE) action_idx = 2;

    if (g_snapshot.combat.pet_goes_first) {
        switch (action) {
            case ACTION_ATTACK:
                g_combat_log.attack_count++;
                combat_engine_player_attack(&g_snapshot.pet, target, &g_snapshot.combat);
                break;
            case ACTION_DEFEND:
                g_combat_log.defend_count++;
                combat_engine_player_attack_defend(&g_snapshot.pet, target, &g_snapshot.combat);
                break;
            case ACTION_FLEE:
                g_combat_log.flee_count++;
                if (combat_engine_try_flee(&g_snapshot.pet, &g_snapshot.encounter, &g_snapshot.combat)) {
                    g_combat_log.flee_success++;
                }
                break;
            case ACTION_SKILL:
                g_combat_log.attack_count++;
                combat_engine_player_attack(&g_snapshot.pet, target, &g_snapshot.combat);
                break;
            default:
                combat_engine_player_attack(&g_snapshot.pet, target, &g_snapshot.combat);
                break;
        }

        if (!g_snapshot.combat.fled && target->alive && g_snapshot.pet.is_alive) {
            combat_engine_enemy_attack(target, &g_snapshot.pet, &g_snapshot.combat);
        }
    } else {
        combat_engine_enemy_attack(target, &g_snapshot.pet, &g_snapshot.combat);
        if (!g_snapshot.combat.fled && g_snapshot.pet.is_alive && target->alive) {
            switch (action) {
                case ACTION_ATTACK:
                    g_combat_log.attack_count++;
                    combat_engine_player_attack(&g_snapshot.pet, target, &g_snapshot.combat);
                    break;
                case ACTION_DEFEND:
                    g_combat_log.defend_count++;
                    combat_engine_player_attack_defend(&g_snapshot.pet, target, &g_snapshot.combat);
                    break;
                case ACTION_FLEE:
                    g_combat_log.flee_count++;
                    if (combat_engine_try_flee(&g_snapshot.pet, &g_snapshot.encounter, &g_snapshot.combat)) {
                        g_combat_log.flee_success++;
                    }
                    break;
                case ACTION_SKILL:
                    g_combat_log.attack_count++;
                    combat_engine_player_attack(&g_snapshot.pet, target, &g_snapshot.combat);
                    break;
                default:
                    combat_engine_player_attack(&g_snapshot.pet, target, &g_snapshot.combat);
                    break;
            }
        }
    }

    try_second_wind();

    g_combat_log.total_damage_dealt += g_snapshot.combat.last_player_damage;
    g_combat_log.total_damage_taken += g_snapshot.combat.last_enemy_damage;

    int16_t pet_hp_after = g_snapshot.pet.hp;
    int16_t enemy_hp_after = target->hp;

    float state_after[9];
    calc_combat_inputs(&g_snapshot.pet, target, state_after, g_last_quality_score);

    reward_context_t reward_ctx = {
        .damage_dealt = g_snapshot.combat.last_player_damage,
        .damage_taken = g_snapshot.combat.last_enemy_damage,
        .pet_hp_before = pet_hp_before,
        .pet_hp_after = pet_hp_after,
        .pet_hp_max = g_snapshot.pet.hp_max,
        .enemy_hp_before = enemy_hp_before,
        .enemy_hp_after = enemy_hp_after,
        .enemy_hp_max = target->hp_max,
        .enemy_killed = !target->alive,
        .pet_died = !g_snapshot.pet.is_alive,
        .fled = g_snapshot.combat.fled,
        .action_taken = action_idx
    };
    float reward = reward_calculator_calc(&reward_ctx);

    bool done = !g_snapshot.pet.is_alive || combat_engine_all_enemies_dead(&g_snapshot.encounter);
    storage_replay_append(state_before, action_idx, reward, state_after, done);

    combat_turn_t turn = {
        .hit = g_snapshot.combat.player_hit,
        .p_success = g_snapshot.combat.last_p_success,
        .dmg_real = (float)g_snapshot.combat.last_player_damage,
        .dmg_min = g_snapshot.combat.last_dmg_min,
        .dmg_max = g_snapshot.combat.last_dmg_max,
        .my_hp = (float)g_snapshot.pet.hp,
        .enemy_hp = (float)target->hp,
        .my_damage_taken = (float)g_snapshot.combat.last_enemy_damage,
        .action_taken = action_idx
    };
    critic_history_add_turn(&g_combat_history, &turn);

    float critic_features[CRITIC_FEATURE_COUNT];
    critic_calc_features(&g_combat_history, critic_features);

    if (g_critic_model.loaded) {
        critic_model_forward(&g_critic_model, critic_features, &g_last_quality_score);
    } else {
        g_last_quality_score = 0.0f;
    }

    state_after[8] = g_last_quality_score;

    experience_logger_log_step(state_before, action_idx, reward, state_after, done);

    ESP_LOGI(TAG, "Transition logged: action=%d reward=%.4f done=%d quality=%.3f total=%lu",
             action_idx, reward, done, g_last_quality_score, (unsigned long)storage_replay_get_total());

    g_combat_result.turns_this_frame++;
    g_combat_result.new_data = true;
}

static void finish_combat(void)
{
uint32_t total_exp = 0;
uint8_t enemies_killed = 0;

int16_t enemy_hp_end = 0;
for (uint8_t i = 0; i < g_snapshot.encounter.count; i++) {
    enemy_hp_end = g_snapshot.encounter.enemies[i].hp;
    if (!g_snapshot.encounter.enemies[i].alive) {
        total_exp += g_snapshot.encounter.enemies[i].exp_reward;
        enemies_killed++;
    }
}

const char *outcome = (enemies_killed > 0) ? "VICTORY" : "DEFEAT";
ESP_LOGI(TAG, "=== COMBAT SUMMARY ===");
ESP_LOGI(TAG, "Enemy: %s HP %d->%d (%s) | Pet: HP %d->%d",
         g_combat_log.enemy_name,
         g_combat_log.enemy_hp_start, enemy_hp_end, outcome,
         g_combat_log.pet_hp_start, g_snapshot.pet.hp);
ESP_LOGI(TAG, "Turns: %d | ATK:%d DEF:%d FLEE:%d(%d ok) | DMG dealt:%d taken:%d | EXP:%d",
         g_combat_log.turn_count,
         g_combat_log.attack_count, g_combat_log.defend_count,
         g_combat_log.flee_count, g_combat_log.flee_success,
         g_combat_log.total_damage_dealt, g_combat_log.total_damage_taken,
         g_combat_log.turn_count);
ESP_LOGI(TAG, "======================");

memset(&g_combat_log, 0, sizeof(g_combat_log));

g_snapshot.pet.enemies_killed += enemies_killed;

for (uint8_t i = 0; i < enemies_killed; i++) {
if (rules_random_chance(DP_GAIN_CHANCE)) {
g_snapshot.pet.dp++;
ESP_LOGI(TAG, "Gained 1 DP! (total: %lu)", (unsigned long)g_snapshot.pet.dp);
}
}

resources_recover_short_rest(&g_snapshot.pet);

g_snapshot.pet.dirty_flags |= PET_DIRTY_DP | PET_DIRTY_RESOURCES;

g_combat_result.combat_ended = true;
g_combat_result.victory = combat_engine_all_enemies_dead(&g_snapshot.encounter);
g_combat_result.new_data = true;

experience_logger_end_episode(g_combat_result.victory, g_snapshot.encounter.enemies[0].level);

if (g_combat_result.victory) {
    uint32_t total_trans = storage_replay_get_total();

    if (check_level_up_trigger(g_snapshot.pet.level, total_trans)) {
        transition_to(GS_TRAINING);
    } else if (rest_should_rest(g_snapshot.pet.hp, g_snapshot.pet.hp_max, g_snapshot.pet.rest.hp_rest_threshold)) {
        rest_init(&g_snapshot.rest, &g_snapshot.pet);
        transition_to(GS_RESTING);
    } else {
        transition_to(GS_VICTORY);
    }
} else {
transition_to(GS_DEAD);
}
}

void game_coordinator_task(void *arg)
{
    ESP_LOGI(TAG, "Coordinator task started");

    while (!g_initialized) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    load_transition_intervals();

    while (1) {
        switch (g_snapshot.state) {
            case GS_INIT:
                if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100))) {
                    if (!storage_load_pet(&g_snapshot.pet)) {
                        strncpy(g_snapshot.pet.name, "Mistolito", PET_NAME_MAX_LEN);
                        g_snapshot.pet.level = 1;
                        g_snapshot.pet.profession = PROF_NONE;
                        g_snapshot.pet.profession_level = 0;

                        storage_load_dna_codes_only(&g_snapshot.pet.dna);
                        g_snapshot.pet.dna.salt = esp_random();
                        storage_derive_dna_stats(&g_snapshot.pet.dna);

                        g_snapshot.pet.str = g_snapshot.pet.dna.base_stats[STAT_STR];
                        g_snapshot.pet.dex = g_snapshot.pet.dna.base_stats[STAT_DEX];
                        g_snapshot.pet.con = g_snapshot.pet.dna.base_stats[STAT_CON];
                        g_snapshot.pet.intel = g_snapshot.pet.dna.base_stats[STAT_INT];
                        g_snapshot.pet.wis = g_snapshot.pet.dna.base_stats[STAT_WIS];
                        g_snapshot.pet.cha = g_snapshot.pet.dna.base_stats[STAT_CHA];

                        storage_apply_profession_data(&g_snapshot.pet, PROF_NONE);
                        g_snapshot.pet.exp_next = calc_exp_for_level(1);
                        g_snapshot.pet.is_alive = true;
                        g_snapshot.pet.dirty_flags = 0xFF;
                        storage_save_pet_delta(g_snapshot.pet.dirty_flags, &g_snapshot.pet);
                        g_snapshot.pet.dirty_flags = 0;

                        ESP_LOGI(TAG, "New pet created with salt 0x%08lX", (unsigned long)g_snapshot.pet.dna.salt);
                    }

        g_snapshot.pet.exp_next = calc_exp_for_level(g_snapshot.pet.level);

        g_search_start_ms = 0;
        g_search_duration_ms = 0;

        if (g_snapshot.pet.energy > 0) {
            transition_to(GS_SEARCHING);
        } else {
            rest_init(&g_snapshot.rest, &g_snapshot.pet);
            transition_to(GS_RESTING);
        }

        if (!inference_engine_is_loaded()) {
            esp_err_t ret = inference_engine_load_model("/sdcard/MODELS/backbone.espdl");
            if (ret != ESP_OK) {
                ESP_LOGW(TAG, "Combat model not loaded, using fallback logic: %s", esp_err_to_name(ret));
            }
        }

        storage_replay_init();
    }
    break;

case GS_SEARCHING:
    if (g_search_start_ms == 0) {
        g_search_start_ms = (uint32_t)(esp_timer_get_time() / 1000);
        g_search_duration_ms = (esp_random() % 5 + 3) * 1000;
        memset(&g_search_result, 0, sizeof(g_search_result));
        ESP_LOGI(TAG, "Searching started, duration: %lu ms", (unsigned long)g_search_duration_ms);
    }

    uint32_t elapsed_ms = (uint32_t)(esp_timer_get_time() / 1000) - g_search_start_ms;
    g_search_result.new_data = true;

    if (elapsed_ms >= g_search_duration_ms) {
        combat_engine_start_encounter(&g_snapshot.encounter, g_snapshot.pet.level);

        g_snapshot.pet.energy--;
        g_snapshot.pet.dirty_flags |= PET_DIRTY_ENERGY;

        g_search_result.search_ended = true;
g_search_result.encounter_found = true;
g_search_result.new_data = true;

ESP_LOGI(TAG, "Encounter spawned: %d enemy(s)", g_snapshot.encounter.count);

uint8_t target_idx = combat_engine_select_first_alive(&g_snapshot.encounter);
enemy_t *first_enemy = &g_snapshot.encounter.enemies[target_idx];
combat_engine_roll_initiative(&g_snapshot.pet, first_enemy, &g_snapshot.combat);

critic_history_init(&g_combat_history, (float)g_snapshot.pet.hp_max, (float)first_enemy->hp_max);
g_last_quality_score = 0.0f;

memset(&g_combat_result, 0, sizeof(g_combat_result));
memset(&g_combat_log, 0, sizeof(g_combat_log));
g_combat_active = true;

experience_logger_start_episode();

g_search_start_ms = 0;
g_search_duration_ms = 0;

emit_enemy_spawned_event();
transition_to(GS_COMBAT);
    } else {
        taskYIELD();
    }
    break;

            case GS_COMBAT:
                if (g_snapshot.combat.fled) {
                    ESP_LOGD(TAG, "GS_COMBAT: flee branch (pet_alive=%d)", g_snapshot.pet.is_alive);
                    experience_logger_end_episode(false, g_snapshot.encounter.enemies[0].level);
                    g_snapshot.combat.fled = false;
                    g_combat_active = false;
                    g_search_start_ms = 0;
                    transition_to(GS_SEARCHING);
                    break;
                }
                {
                    bool pet_dead = !g_snapshot.pet.is_alive;
                    bool all_enemies_dead = combat_engine_all_enemies_dead(&g_snapshot.encounter);
                    if (pet_dead || all_enemies_dead) {
                        ESP_LOGD(TAG, "GS_COMBAT: end branch pet_dead=%d all_enemies_dead=%d active=%d",
                                 pet_dead, all_enemies_dead, g_combat_active);
                        if (g_combat_active) {
                            finish_combat();
                            g_combat_active = false;
                        }
                    } else {
                        simulate_combat_turn();
                        taskYIELD();
                    }
                }
                break;

case GS_VICTORY:
    storage_save_pet_delta(g_snapshot.pet.dirty_flags, &g_snapshot.pet);
    g_snapshot.pet.dirty_flags = 0;
    g_search_start_ms = 0;
    if (rest_should_rest(g_snapshot.pet.hp, g_snapshot.pet.hp_max, g_snapshot.pet.rest.hp_rest_threshold)) {
        rest_init(&g_snapshot.rest, &g_snapshot.pet);
        transition_to(GS_RESTING);
    } else if (g_snapshot.pet.energy > 0) {
        transition_to(GS_SEARCHING);
    } else {
        rest_init(&g_snapshot.rest, &g_snapshot.pet);
        transition_to(GS_RESTING);
    }
    break;

case GS_TRAINING:
{
    uint32_t total_before_training = storage_replay_get_total();
    ESP_LOGI(TAG, "Starting PPO training... total_from_storage=%lu", (unsigned long)total_before_training);

    training_mode_update_status("Entrenando modelo...");

    uint8_t num_actions = dna_engine_get_unlocked_actions(&g_snapshot.pet.dna, g_snapshot.pet.level);

    if (g_policy_head.num_actions == 0) {
        esp_err_t ret = policy_head_init(&g_policy_head, num_actions);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Policy head init failed: %s", esp_err_to_name(ret));
            transition_to(GS_SEARCHING);
            break;
        }
        policy_head_load(&g_policy_head);
    }

    if (num_actions > g_policy_head.num_actions) {
        ESP_LOGI(TAG, "Expanding policy head: %d -> %d actions",
                 g_policy_head.num_actions, num_actions);
        policy_head_expand(&g_policy_head, num_actions);
        storage_replay_set_action_count(num_actions);
    }

    ppo_config_t config = ppo_config_default();
    training_progress_t progress = {0};

    esp_err_t ret = ppo_train_policy(&g_policy_head, &config, &progress);

    if (ret == ESP_OK && progress.accepted) {
        ESP_LOGI(TAG, "Training done, loss=%.4f", progress.loss);
        training_mode_update_status("Entrenamiento completo!");

        uint32_t next_epoch = 0;
        ppo_find_latest_checkpoint(&next_epoch);
        ppo_save_checkpoint(&g_policy_head, next_epoch + 1, progress.loss);

        policy_head_save(&g_policy_head, next_epoch + 1);

        process_level_up();
        ESP_LOGI(TAG, "Level up! Pet is now level %d", g_snapshot.pet.level);
    } else {
        ESP_LOGW(TAG, "Training failed: staying at level %d (need %d more transitions)",
                 g_snapshot.pet.level, 50);
        training_mode_update_status("Entrenamiento fallido - reintentando...");
    }

    vTaskDelay(pdMS_TO_TICKS(1000));

    if (rest_should_rest(g_snapshot.pet.hp, g_snapshot.pet.hp_max, g_snapshot.pet.rest.hp_rest_threshold)) {
        rest_init(&g_snapshot.rest, &g_snapshot.pet);
        transition_to(GS_RESTING);
    } else if (g_snapshot.pet.energy > 0) {
        transition_to(GS_SEARCHING);
    } else {
        rest_init(&g_snapshot.rest, &g_snapshot.pet);
        transition_to(GS_RESTING);
    }
}
break;

        case GS_RESTING:
        {
            int16_t hp_rec = rest_tick_hp(&g_snapshot.pet, &g_snapshot.rest);
            int16_t en_rec = rest_tick_energy(&g_snapshot.pet, &g_snapshot.rest);

            g_rest_result.hp_recovered += hp_rec;
            g_rest_result.energy_recovered += en_rec;
            g_rest_result.new_data = true;

            g_snapshot.pet.dirty_flags |= PET_DIRTY_HP | PET_DIRTY_ENERGY;

            bool hp_done = (g_snapshot.rest.hp_ticks_remaining == 0);
            bool en_done = (g_snapshot.rest.energy_ticks_remaining == 0);

            if (hp_done && en_done) {
                rest_finish(&g_snapshot.pet);
                g_snapshot.pet.dirty_flags |= PET_DIRTY_RESOURCES;
                storage_save_pet_delta(g_snapshot.pet.dirty_flags, &g_snapshot.pet);
                g_snapshot.pet.dirty_flags = 0;

                g_rest_result.rest_ended = true;
                g_rest_result.new_data = true;

                transition_to(GS_SEARCHING);
            } else {
                taskYIELD();
            }
        }
break;

        case GS_DEAD:
        {
            uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
            uint32_t elapsed = now - g_snapshot.state_entered_ms;
            if (elapsed < 3000) {
                vTaskDelay(pdMS_TO_TICKS(100));
                break;
            }

            ESP_LOGI(TAG, "Pet died! Resetting...");
            if (g_snapshot_mutex && xSemaphoreTake(g_snapshot_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
                combat_engine_reset_pet_on_death(&g_snapshot.pet);
                storage_apply_profession_data(&g_snapshot.pet, PROF_NONE);
                g_snapshot.pet.exp_next = calc_exp_for_level(1);
                g_snapshot.pet.dirty_flags = 0xFF;
                xSemaphoreGive(g_snapshot_mutex);
            }
            storage_save_pet_delta(g_snapshot.pet.dirty_flags, &g_snapshot.pet);
            g_snapshot.pet.dirty_flags = 0;
            storage_replay_reset();
            storage_checkpoints_reset();
            g_search_start_ms = 0;
            transition_to(GS_SEARCHING);
            break;
        }

        default:
                transition_to(GS_INIT);
                break;
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
