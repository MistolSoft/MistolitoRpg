#ifndef SEARCH_ENGINE_H
#define SEARCH_ENGINE_H

#include "mistolito.h"
#include "game_tables_structs.h"

typedef struct {
    int16_t x;
    int16_t y;
    uint8_t density;
    uint8_t detect_dc;
} search_radar_cell_t;

typedef struct {
    uint8_t radius;
    uint8_t cell_count;
    uint8_t current_zone_id;
    char zone_name[16];
    search_radar_cell_t cells[100];
} search_radar_t;

void search_engine_init(pet_t *pet);
void search_engine_generate_radar(const pet_t *pet, search_radar_t *radar);
void search_engine_tick(pet_t *pet, search_frame_result_t *result);
void search_engine_materialize_tile(int16_t x, int16_t y, tile_record_t *tile);

#endif
