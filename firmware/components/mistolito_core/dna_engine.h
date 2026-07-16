#ifndef DNA_ENGINE_H
#define DNA_ENGINE_H

#include <stdint.h>
#include <stdbool.h>

#ifndef DNA_TYPES_DEFINED
#define DNA_TYPES_DEFINED

#define DNA_CODE_LEN 7
#define DNA_HASH_LEN 32
#define DNA_STAT_COUNT 6

#define DNA_CAP_OFFSET 5

#define DNA_MAX_SKILL_SLOTS 8

typedef struct {
    char codes[DNA_STAT_COUNT][DNA_CODE_LEN];
    uint8_t hash[DNA_HASH_LEN];
    uint32_t salt;
    uint8_t base_stats[DNA_STAT_COUNT];
    uint8_t caps[DNA_STAT_COUNT];
    uint8_t max_skills;
    uint8_t skill_slot_levels[DNA_MAX_SKILL_SLOTS];
} dna_t;

typedef struct {
    uint8_t candidates[DNA_STAT_COUNT];
    uint8_t count;
} levelup_queue_t;

typedef struct {
    dna_t *dna;
    uint8_t stat_idx;
    uint8_t enemy_id;
    uint8_t round;
    const char *action;
} roll_context_t;

#endif

#include "esp_err.h"

esp_err_t dna_init(dna_t *dna);
void dna_generate_hash(dna_t *dna);
void dna_derive_all_stats(dna_t *dna, uint8_t stats[DNA_STAT_COUNT]);
void dna_derive_skill_slots(dna_t *dna);
uint8_t dna_derive_single_stat(uint8_t *hash, uint8_t stat_idx);
uint8_t dna_roll_d20(dna_t *dna, uint8_t stat_idx, uint32_t action_salt);
uint8_t dna_roll_d20_context(roll_context_t *ctx);
uint8_t dna_get_cap(dna_t *dna, uint8_t stat_idx);
int8_t dna_get_modifier(uint8_t stat_value);
bool dna_check_stat_increase(dna_t *dna, uint8_t stat_idx, uint8_t current_value, uint8_t level);
levelup_queue_t dna_get_levelup_candidates(dna_t *dna, uint8_t current_stats[DNA_STAT_COUNT], uint8_t level);
void dna_apply_levelup(dna_t *dna, uint8_t stats[DNA_STAT_COUNT], levelup_queue_t *queue);
uint8_t dna_get_open_skill_slots(const dna_t *dna, uint8_t level);

#endif
