#ifndef SKILLS_PERKS_ENGINE_H
#define SKILLS_PERKS_ENGINE_H

#include "mistolito.h"
#include "esp_err.h"

#define MAX_LEARN_PER_LEVEL 2

typedef enum {
    LEARN_TYPE_SKILL,
    LEARN_TYPE_PERK
} learn_type_e;

typedef struct {
    learn_type_e type;
    uint8_t id;
    uint8_t intent_type;
    uint8_t dp_cost;
    uint8_t success_dc;
} learn_candidate_t;

bool skill_check_requirements(pet_t *pet, uint8_t skill_id, uint8_t profession_level);
bool perk_check_requirements(pet_t *pet, uint8_t perk_id, uint8_t profession_level);
void skills_perks_get_available(pet_t *pet, uint8_t profession_level, learn_candidate_t *candidates, uint8_t *count);
bool skills_perks_try_learn(pet_t *pet, learn_candidate_t *candidate);
void skills_perks_process_level_up(pet_t *pet, uint8_t profession_level);
bool skill_is_known(pet_t *pet, uint8_t skill_id);
bool perk_is_known(pet_t *pet, uint8_t perk_id);

#endif
