#ifndef EXPERIENCE_LOGGER_H
#define EXPERIENCE_LOGGER_H

#include "storage_task.h"
#include <stdint.h>
#include <stdbool.h>

#define EPISODE_MAX_TURNS 64

void experience_logger_start_episode(void);
void experience_logger_log_step(float *state, uint8_t action, float reward, float *next_state, uint8_t done);
void experience_logger_end_episode(bool victory, uint8_t enemy_level);
uint32_t experience_logger_get_total_episodes(void);

#endif
