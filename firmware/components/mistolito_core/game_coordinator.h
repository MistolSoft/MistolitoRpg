#ifndef GAME_COORDINATOR_H
#define GAME_COORDINATOR_H

#include "mistolito.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

void game_coordinator_start(void);
game_snapshot_t* game_coordinator_get_snapshot(void);
SemaphoreHandle_t game_coordinator_get_mutex(void);
QueueHandle_t game_coordinator_get_turn_done_queue(void);
combat_frame_result_t* game_coordinator_get_combat_result(void);
void game_coordinator_clear_combat_result(void);
search_frame_result_t* game_coordinator_get_search_result(void);
void game_coordinator_clear_search_result(void);
rest_frame_result_t* game_coordinator_get_rest_result(void);
void game_coordinator_clear_rest_result(void);
void game_coordinator_task(void *arg);
uint32_t game_coordinator_get_trans_for_level(uint8_t level);

extern TaskHandle_t g_coordinator_task_handle;

#endif
