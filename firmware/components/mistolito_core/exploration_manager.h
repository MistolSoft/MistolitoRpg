#ifndef EXPLORATION_MANAGER_H
#define EXPLORATION_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include <esp_err.h>
#include "mistolito.h"

#define EXPLORATION_MAGIC       0x4558504C
#define EXPLORATION_VERSION     2
#define EXPLORATION_FORCED_USES 30
#define EXPLORATION_FORCE_PCT   40

#define EXPLORATION_FILE        "/sdcard/BRAIN/COMBAT/exploration/counters.bin"

typedef struct {
    uint32_t uses_remaining[MAX_SKILLS];
    uint32_t total_forced[MAX_SKILLS];
} __attribute__((packed)) exploration_state_t;

esp_err_t exploration_manager_init(void);
bool exploration_manager_should_force(const pet_t *pet, uint8_t *out_nn_index);
void exploration_manager_on_use(uint8_t skill_slot);
void exploration_manager_skill_learned(uint8_t skill_slot);
bool exploration_manager_has_pending(uint8_t skill_count);
esp_err_t exploration_manager_save(void);
esp_err_t exploration_manager_load(void);

#endif
