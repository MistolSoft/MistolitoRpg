#ifndef SCREENS_H
#define SCREENS_H

#include "mistolito.h"
#include "lvgl.h"

typedef enum {
    SCREEN_INIT,
    SCREEN_GAME,
    SCREEN_SEARCHING,
    SCREEN_REST,
    SCREEN_DEATH,
    SCREEN_COUNT
} screen_id_e;

void screens_init(void);
void screens_load(screen_id_e id);
void screens_update(game_snapshot_t *snap);
screen_id_e screens_get_current(void);

void screens_show_damage_popup(int16_t damage, bool is_critical);
void screens_show_miss(void);
void screens_clear_damage_popup(void);
void screens_show_exp_popup(uint32_t exp);
void screens_clear_exp_popup(void);
void screens_set_pet_animation(int anim_type);
void screens_set_enemy_animation(int anim_type);
void screens_show_rest_hp_popup(int16_t hp);
void screens_show_rest_en_popup(int16_t en);
void screens_clear_rest_popups(void);
void screens_set_init_hint(const char *text);

#endif
