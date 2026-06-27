#ifndef COMBAT_ENGINE_H
#define COMBAT_ENGINE_H

#include "mistolito.h"
#include <stdint.h>
#include <stdbool.h>

void combat_engine_init(combat_state_t *combat);
void combat_engine_start_encounter(encounter_t *encounter, uint8_t pet_level);
void combat_engine_roll_initiative(pet_t *pet, enemy_t *enemy, combat_state_t *combat);
bool combat_engine_saving_throw(pet_t *pet, uint8_t save_type, uint8_t dc);
bool combat_engine_enemy_saving_throw(enemy_t *enemy, uint8_t save_type, uint8_t dc);
void combat_engine_player_attack(pet_t *pet, enemy_t *target, combat_state_t *combat);
void combat_engine_player_attack_defend(pet_t *pet, enemy_t *target, combat_state_t *combat);
bool combat_engine_try_flee(pet_t *pet, encounter_t *encounter, combat_state_t *combat);
void combat_engine_enemy_attack(enemy_t *enemy, pet_t *pet, combat_state_t *combat);
void combat_engine_log_round(pet_t *pet, enemy_t *enemy, combat_state_t *combat);
bool combat_engine_all_enemies_dead(encounter_t *encounter);
void combat_engine_apply_round_effects(pet_t *pet, encounter_t *encounter);
uint8_t combat_engine_select_target_ai(pet_t *pet, encounter_t *encounter);
uint8_t combat_engine_select_first_alive(encounter_t *encounter);

uint16_t combat_engine_calc_enemy_hp(uint8_t pet_level);
uint8_t combat_engine_calc_enemy_ac(uint8_t pet_level);

void combat_engine_reset_pet_on_death(pet_t *pet);

#endif
