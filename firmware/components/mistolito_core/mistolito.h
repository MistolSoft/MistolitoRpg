#ifndef MISTOLITO_H
#define MISTOLITO_H

#include <stdint.h>
#include <stdbool.h>
#include "dna_engine.h"

#define MISTOLITO_VERSION "2.0.0"

#define LCD_WIDTH 320
#define LCD_HEIGHT 240

#define MAX_ENERGY 10
#define MAX_NAME_LEN 16
#define MAX_SKILLS 32
#define SKILL_NAME_MAX_LEN 16
#define PET_NAME_MAX_LEN 16
#define ENEMY_NAME_MAX_LEN 16
#define MAX_ENEMIES_PER_ENCOUNTER 3

#define STAT_STR 0
#define STAT_DEX 1
#define STAT_CON 2
#define STAT_INT 3
#define STAT_WIS 4
#define STAT_CHA 5
#define STAT_COUNT 6

#define PROF_NONE 0
#define PROF_WARRIOR 1
#define PROF_MAGE 2
#define PROF_ROGUE 3
#define PROF_COUNT 4

#define BASE_PET_HP 20
#define BASE_ENEMY_HP 50
#define ENEMY_HP_PER_LEVEL 20
#define BASE_ENEMY_AC 8
#define DP_GAIN_CHANCE 30
#define PROFESSION_UNLOCK_DP 10

#define REST_BASE_TICKS 5
#define REST_DICE_SIDES 5
#define REST_RECOVERY_PERCENT 10

#define MAX_STATUS_EFFECTS 8
#define MAX_PERKS 16
#define MAX_SPELLS_KNOWN 32
#define MAX_MANEUVERS_KNOWN 16

#define STATUS_NONE      0
#define STATUS_POISON    (1 << 0)
#define STATUS_STUNNED   (1 << 1)
#define STATUS_BLESSED   (1 << 2)
#define STATUS_CURSED    (1 << 3)

typedef struct {
    uint8_t hp_rest_threshold;
    uint8_t recovery_chance;
} pet_rest_t;

typedef struct {
    uint8_t base_ac;
    uint8_t damage_dice;
    uint8_t damage_bonus;
    uint8_t dice_count;
} pet_combat_t;

typedef struct {
    int8_t min_damage;
    int8_t max_damage;
    int8_t extra_dice;
    int8_t crit;
    int8_t sneak_dice;
    int8_t skill_uses;
} pet_bonuses_t;

typedef struct {
    uint8_t id;
    uint8_t stacks;
} status_effect_t;

typedef struct {
    uint8_t id;
} perk_t;

typedef struct {
    uint8_t skill_id;
    uint8_t intent_type;
    uint8_t uses_remaining;
    uint8_t uses_max;
} pet_skill_t;

typedef struct {
char id[32];
uint8_t level;
} pet_spell_t;

typedef struct {
uint8_t maneuver_id;
} pet_maneuver_t;

#define MAX_ABILITY_POINTS 10

typedef struct {
uint8_t slots[9];
} spell_slots_t;

typedef enum {
    GS_INIT,
    GS_SEARCHING,
    GS_COMBAT,
    GS_VICTORY,
    GS_LEVELUP,
    GS_TRAINING,
    GS_RESTING,
    GS_DEAD
} game_state_e;

typedef enum {
    CS_INIT,
    CS_PLAYER_SELECT_ACTION,
    CS_PLAYER_SELECT_TARGET,
    CS_PLAYER_EXECUTE,
    CS_ENEMY_SELECT_ACTION,
    CS_ENEMY_EXECUTE,
    CS_ROUND_END,
    CS_CHECK_VICTORY
} combat_phase_e;

typedef enum {
    ACTION_NONE = 0,
    ACTION_ATTACK,
    ACTION_SKILL,
    ACTION_DEFEND,
    ACTION_FLEE
} combat_action_e;

typedef enum {
    COMBAT_RESULT_MISS = 0,
    COMBAT_RESULT_HIT = 1,
    COMBAT_RESULT_ENEMY_DEAD = 0xFFFF
} combat_result_e;

#define PET_DIRTY_NAME (1 << 0)
#define PET_DIRTY_LEVEL (1 << 1)
#define PET_DIRTY_HP (1 << 3)
#define PET_DIRTY_ENERGY (1 << 4)
#define PET_DIRTY_STATS (1 << 5)
#define PET_DIRTY_PROFESSION (1 << 6)
#define PET_DIRTY_DP (1 << 7)
#define PET_DIRTY_PROF_LEVEL (1 << 8)
#define PET_DIRTY_SKILLS (1 << 9)
#define PET_DIRTY_PERKS (1 << 10)
#define PET_DIRTY_SPELLS (1 << 11)
#define PET_DIRTY_RESOURCES (1 << 12)
#define PET_DIRTY_MANEUVERS (1 << 13)

typedef struct {
    char name[PET_NAME_MAX_LEN];
    uint8_t level;
    uint8_t profession_level;
    uint32_t exp;
    uint32_t exp_next;
    int16_t hp;
    int16_t hp_max;
    uint8_t str;
    uint8_t dex;
    uint8_t con;
    uint8_t intel;
    uint8_t wis;
    uint8_t cha;
    uint8_t profession;
    uint32_t dp;
    uint32_t enemies_killed;
    uint8_t lives;
    uint8_t energy;
    uint8_t energy_max;
    dna_t dna;
    pet_rest_t rest;
    pet_combat_t combat;
    pet_bonuses_t bonuses;
    status_effect_t status[MAX_STATUS_EFFECTS];
    uint8_t status_count;
perk_t perks[MAX_PERKS];
uint8_t perk_count;
pet_skill_t skills[MAX_SKILLS];
uint8_t skill_count;
spell_slots_t spell_slots;
uint8_t spell_slots_max[9];
pet_spell_t spells_known[MAX_SPELLS_KNOWN];
uint8_t spells_known_count;
uint8_t cantrips_known;
uint8_t cantrips_max;
pet_maneuver_t maneuvers[MAX_MANEUVERS_KNOWN];
uint8_t maneuver_count;
uint8_t superiority_dice;
uint8_t superiority_dice_max;
uint8_t superiority_dice_size;
uint8_t action_surge_uses;
uint8_t action_surge_max;
uint8_t indomitable_uses;
uint8_t indomitable_max;
uint8_t second_wind_uses;
uint8_t sneak_attack_dice;
uint8_t arcane_recovery_used;
uint8_t ability_points;
    uint8_t ability_points_max;
    uint16_t dirty_flags;
bool is_alive;
} pet_t;

typedef struct {
char name[ENEMY_NAME_MAX_LEN];
int16_t hp;
int16_t hp_max;
uint8_t ac;
uint8_t attack_bonus;
uint8_t damage_dice;
uint8_t damage_bonus;
uint8_t level;
uint16_t exp_reward;
int8_t initiative;
uint8_t str_save;
uint8_t dex_save;
uint8_t con_save;
bool alive;
} enemy_t;

typedef struct {
    enemy_t enemies[MAX_ENEMIES_PER_ENCOUNTER];
    uint8_t count;
    uint8_t active_idx;
} encounter_t;

typedef struct {
    combat_phase_e phase;
    uint8_t round;
    uint8_t current_enemy_idx;
    combat_action_e selected_action;
    uint8_t selected_target_idx;
    int16_t last_player_damage;
    int16_t last_enemy_damage;
    bool player_hit;
    bool enemy_hit;
    bool animation_active;
    uint8_t turn_count;
    int8_t pet_initiative;
    int8_t enemy_initiative;
    bool pet_goes_first;
    bool fled;
    uint8_t defend_bonus_ac;
    float last_p_success;
    float last_dmg_min;
    float last_dmg_max;
} combat_state_t;

typedef struct {
    uint8_t hp_ticks_remaining;
    uint8_t energy_ticks_remaining;
    uint8_t hp_ticks_total;
    uint8_t energy_ticks_total;
} rest_state_t;

typedef struct {
    int16_t pet_damage_this_frame;
    int16_t enemy_damage_this_frame;
    bool pet_hit_this_frame;
    bool enemy_hit_this_frame;
    bool combat_ended;
    bool victory;
    uint8_t turns_this_frame;
    bool new_data;
} combat_frame_result_t;

typedef struct {
    bool search_ended;
    bool encounter_found;
    bool new_data;
} search_frame_result_t;

typedef struct {
    int16_t hp_recovered;
    int16_t energy_recovered;
    bool rest_ended;
    bool new_data;
} rest_frame_result_t;

typedef struct {
    game_state_e state;
    uint32_t state_entered_ms;
    pet_t pet;
    encounter_t encounter;
    combat_state_t combat;
    rest_state_t rest;
} game_snapshot_t;

#endif
