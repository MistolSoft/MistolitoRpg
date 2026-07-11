#ifndef GAME_TABLES_STRUCTS_H
#define GAME_TABLES_STRUCTS_H

#include <stdint.h>

typedef struct {
    uint32_t exp_base;
    uint32_t exp_linear;
    float exp_multiplier;
    uint32_t hp_bonus_base;
    uint32_t hp_bonus_step;
    uint32_t cycle_length;
    uint32_t dp_gain_chance;
    uint32_t profession_unlock_dp;
    uint32_t rest_base_ticks;
    uint32_t rest_dice_sides;
    uint32_t rest_recovery_percent;
    uint32_t max_stats_per_level;
    uint32_t max_skills_per_level;
    uint32_t base_stat_dc;
} __attribute__((packed)) config_record_t;

typedef struct {
    uint8_t id;
    char name[16];
    uint8_t req_str;
    uint8_t req_con;
    uint8_t req_dex;
    uint8_t req_int;
    uint8_t req_wis;
    uint8_t req_cha;
    uint8_t bonus_str;
    uint8_t bonus_con;
    uint8_t bonus_dex;
    uint8_t bonus_int;
    uint8_t bonus_wis;
    uint8_t bonus_cha;
    uint8_t success_dc;
    uint8_t dp_cost;
    uint8_t hp_rest_threshold;
    uint8_t recovery_chance;
    uint8_t base_hp;
    uint8_t base_energy;
    uint8_t base_ac;
    uint8_t damage_dice;
    uint8_t damage_bonus;
    uint8_t dice_count;
    uint8_t hit_dice;
    uint8_t hp_per_level;
} __attribute__((packed)) profession_record_t;

typedef struct {
    uint8_t id;
    char name[16];
    uint8_t tier;
    uint8_t base_hp;
    uint8_t hp_per_level;
    uint8_t base_ac;
    uint8_t ac_per_level;
    uint8_t damage_dice;
    uint8_t damage_bonus;
    uint8_t damage_per_level;
    uint8_t attack_bonus;
    uint16_t exp_base;
    uint16_t exp_per_level;
    uint8_t detect_dc;
} __attribute__((packed)) enemy_record_t;

typedef struct {
    uint8_t id;
    char name[16];
    int16_t min_x;
    int16_t max_x;
    int16_t min_y;
    int16_t max_y;
} __attribute__((packed)) world_zone_record_t;

typedef struct {
    uint8_t zone_id;
    uint8_t enemy_id;
} __attribute__((packed)) zone_enemy_record_t;

typedef struct {
    uint8_t tier;
    uint8_t min_level;
    uint8_t max_level;
} __attribute__((packed)) enemy_tier_record_t;

typedef struct {
    uint32_t max_level;
    uint32_t transitions_per_level;
} __attribute__((packed)) transition_interval_record_t;

typedef struct {
    uint8_t str;
    uint8_t dex;
    uint8_t con;
    uint8_t intel;
    uint8_t wis;
    uint8_t cha;
} __attribute__((packed)) stat_req_t;

typedef struct {
    uint8_t id;
    char name[32];
    uint8_t profession_level_req;
    uint8_t profession_req_mask;
    stat_req_t stat_req;
    uint8_t dp_cost;
    uint8_t success_dc;
    uint8_t intent_type;
} __attribute__((packed)) skill_record_t;

typedef struct {
    uint8_t id;
    char name[32];
    uint8_t profession_level_req;
    uint8_t profession_req_mask;
    stat_req_t stat_req;
    uint8_t dp_cost;
    uint8_t success_dc;
} __attribute__((packed)) perk_record_t;

typedef struct {
    char id[24];
    char name[32];
    uint8_t level;
    uint8_t dp_cost;
    uint8_t success_dc;
} __attribute__((packed)) spell_record_t;

typedef struct {
    uint8_t profession;
    uint8_t level;
    char name[32];
    uint8_t dp_cost;
    uint8_t success_dc;
    char archetype_req[16];
} __attribute__((packed)) feature_record_t;

typedef struct {
    uint8_t action_surge_max;
    uint8_t indomitable_max;
    uint8_t superiority_dice_max;
    uint8_t superiority_dice_size;
} __attribute__((packed)) warrior_lvl_res_t;

typedef struct {
    uint8_t slots[9];
} __attribute__((packed)) mage_lvl_res_t;

typedef struct {
    uint8_t sneak_attack_dice;
} __attribute__((packed)) rogue_lvl_res_t;

typedef struct {
    uint8_t level;
    warrior_lvl_res_t warrior;
    mage_lvl_res_t mage;
    rogue_lvl_res_t rogue;
} __attribute__((packed)) resource_record_t;

typedef struct {
    uint8_t profession;
    uint8_t level;
    uint8_t min_damage;
    uint8_t max_damage;
    uint8_t extra_dice;
    uint8_t crit;
    uint8_t sneak_dice;
    uint8_t skill_uses;
} __attribute__((packed)) damage_progression_record_t;

typedef struct {
    uint8_t biome_id;
    uint8_t densities[3];
} __attribute__((packed)) tile_record_t;

typedef struct {
    tile_record_t tiles[256];
} __attribute__((packed)) chunk_t;

#endif
