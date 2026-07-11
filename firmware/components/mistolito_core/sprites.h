#ifndef SPRITES_H
#define SPRITES_H

#include "lvgl.h"

LV_IMG_DECLARE(p1_walk01);
LV_IMG_DECLARE(p1_walk02);
LV_IMG_DECLARE(p1_walk03);
LV_IMG_DECLARE(p1_walk04);
LV_IMG_DECLARE(p1_walk05);
LV_IMG_DECLARE(p1_walk06);
LV_IMG_DECLARE(p1_walk07);
LV_IMG_DECLARE(p1_walk08);
LV_IMG_DECLARE(p1_walk09);
LV_IMG_DECLARE(p1_walk10);

LV_IMG_DECLARE(slimeWalk1);
LV_IMG_DECLARE(slimeWalk2);
LV_IMG_DECLARE(slimeDead);
LV_IMG_DECLARE(fishSwim1);
LV_IMG_DECLARE(fishSwim2);
LV_IMG_DECLARE(fishDead);
LV_IMG_DECLARE(flyFly1);
LV_IMG_DECLARE(flyFly2);
LV_IMG_DECLARE(flyDead);
LV_IMG_DECLARE(blockerBody);
LV_IMG_DECLARE(blockerMad);
LV_IMG_DECLARE(blockerSad);
LV_IMG_DECLARE(pokerMad);
LV_IMG_DECLARE(pokerSad);

LV_IMG_DECLARE(grassMid);
LV_IMG_DECLARE(grassCenter);
LV_IMG_DECLARE(dirtMid);
LV_IMG_DECLARE(dirtCenter);
LV_IMG_DECLARE(castleCenter);
LV_IMG_DECLARE(stoneCenter);

void sprites_init(void);
void sprites_set_idle_animation(lv_obj_t *animimg);
void sprites_set_walk_animation(lv_obj_t *animimg);
void sprites_set_attack_animation(lv_obj_t *animimg);
void sprites_set_hit_animation(lv_obj_t *animimg);
void sprites_set_death_animation(lv_obj_t *animimg);
void sprites_set_levelup_animation(lv_obj_t *animimg);
void sprites_set_enemy_slime_animation(lv_obj_t *animimg);
void sprites_set_enemy_fish_animation(lv_obj_t *animimg);
void sprites_set_enemy_fly_animation(lv_obj_t *animimg);
void sprites_set_enemy_blocker_animation(lv_obj_t *animimg);
void sprites_set_enemy_poker_animation(lv_obj_t *animimg);
void sprites_stop_animation(lv_obj_t *animimg);
void sprites_create_arena_background(lv_obj_t *parent);

#endif
