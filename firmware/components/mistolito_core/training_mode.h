#ifndef TRAINING_MODE_H
#define TRAINING_MODE_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint8_t current_epoch;
    uint8_t total_epochs;
    float loss;
    uint32_t transitions_processed;
    uint32_t total_transitions;
    uint32_t elapsed_ms;
    bool accepted;
} training_progress_t;

void training_mode_enter(void);
void training_mode_exit(bool accepted);
bool training_mode_is_active(void);
void training_mode_update_progress(const training_progress_t *progress);
void training_mode_update_status(const char *status);
void training_mode_lvgl_tick(void);

#endif
