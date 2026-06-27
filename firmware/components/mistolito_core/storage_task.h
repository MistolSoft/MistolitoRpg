#ifndef STORAGE_TASK_H
#define STORAGE_TASK_H

#include "mistolito.h"
#include "dna_engine.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#define MOUNT_POINT "/sdcard"

#define STORAGE_OP_SAVE_PET_DELTA 1
#define STORAGE_OP_LOAD_PET 2
#define STORAGE_OP_LOAD_TABLES 3
#define STORAGE_OP_REPLAY_APPEND 4
#define STORAGE_OP_REPLAY_READ   5
#define STORAGE_OP_REPLAY_INIT   6

#define REPLAY_MAGIC            0x52504C59  /* "RPLY" */
#define REPLAY_VERSION          1
#define REPLAY_TRANSITIONS_PER_CHUNK  10000
#define REPLAY_TRANSITION_SIZE  78  /* 36+1+4+36+1 = 9+1+4+9+1 floats */

#define COMBAT_DIR MOUNT_POINT "/BRAIN/COMBAT"
#define REPLAY_BASE_DIR COMBAT_DIR "/replay"
#define EPISODES_DIR COMBAT_DIR "/episodes"
#define CHECKPOINTS_DIR COMBAT_DIR "/checkpoints"
#define EXPLORATION_DIR COMBAT_DIR "/exploration"

typedef struct {
    float state[9];
    uint8_t action;
    float reward;
    float next_state[9];
    uint8_t done;
} __attribute__((packed)) replay_transition_t;

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t total_chunks;
    uint32_t total_transitions;
    uint32_t current_chunk_transitions;
    uint32_t checksum;
    uint8_t num_actions;
    uint8_t reserved[3];
} __attribute__((packed)) replay_header_t;

typedef struct {
    uint32_t total_chunks;
    uint32_t total_transitions;
    uint32_t current_chunk_transitions;
    uint8_t current_chunk_index;
} replay_stats_t;

typedef struct {
    uint8_t operation;
    uint8_t dirty_flags;
    pet_t *pet;
} storage_pet_request_t;

typedef struct {
    uint8_t operation;
    replay_transition_t transition;
} storage_replay_request_t;

typedef struct {
    uint8_t operation;
    uint32_t chunk_index;
    uint32_t *out_count;
} storage_replay_read_request_t;

typedef union {
    uint8_t operation;
    storage_pet_request_t pet;
    storage_replay_request_t replay;
    storage_replay_read_request_t replay_read;
} storage_request_t;

#define STORAGE_QUEUE_SIZE 4

extern QueueHandle_t g_storage_queue;

void storage_task_start(void);
void storage_task(void *arg);
void storage_save_pet_delta(uint8_t dirty_flags, pet_t *pet);
bool storage_load_pet(pet_t *pet);
bool storage_load_tables(uint32_t *exp_table, uint16_t *hp_bonus);
bool storage_load_game_tables(void);
bool storage_apply_profession_data(pet_t *pet, uint8_t profession_id);
bool storage_get_enemy_data(uint8_t enemy_id, uint8_t pet_level, enemy_t *enemy);
uint8_t storage_get_random_enemy_id(uint8_t pet_level);
bool storage_file_exists(const char *path);
esp_err_t storage_save_file(const char *path, const uint8_t *data, size_t len);
esp_err_t storage_delete_file(const char *path);
bool storage_load_dna_codes_only(dna_t *dna);
void storage_derive_dna_stats(dna_t *dna);
const char* storage_get_professions_json(void);
const char* storage_get_game_tables_json(void);

void storage_replay_init(void);
void storage_replay_set_action_count(uint8_t num_actions);
uint8_t storage_replay_get_action_count(void);
void storage_replay_reset(void);
void storage_checkpoints_reset(void);
void storage_replay_append(float *state, uint8_t action, float reward, float *next_state, uint8_t done);
bool storage_replay_read_chunk(uint32_t chunk_index, replay_transition_t *out, uint32_t *out_count);
bool storage_replay_get_stats(replay_stats_t *stats);
void storage_replay_get_header_path(char *out_path, size_t max_len);
void storage_replay_get_dir_path(char *out_path, size_t max_len);
uint32_t storage_replay_get_total(void);
uint32_t storage_replay_calc_transitions_for_level(uint8_t current_level);
bool storage_replay_check_level_up(uint8_t current_level);

#endif
