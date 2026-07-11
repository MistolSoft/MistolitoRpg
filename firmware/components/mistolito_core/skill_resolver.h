#ifndef SKILL_RESOLVER_H
#define SKILL_RESOLVER_H

#include <stdint.h>
#include <stdbool.h>
#include "mistolito.h"

#define INTENT_ATTACK   0
#define INTENT_DEFEND   1
#define INTENT_HEAL     2
#define INTENT_MAGIC    3
#define INTENT_SUPPORT  4
#define INTENT_FLEE     5

uint8_t skill_resolver_resolve(const pet_t *pet, uint8_t intent);
bool skill_resolver_has_skill_for_intent(const pet_t *pet, uint8_t intent);
uint8_t skill_resolver_count_for_intent(const pet_t *pet, uint8_t intent);

#endif
