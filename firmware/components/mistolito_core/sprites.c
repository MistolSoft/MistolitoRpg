#include "sprites.h"
#include "ui_colors.h"
#include "esp_log.h"

static const char *TAG = "SPRITES";

static lv_image_dsc_t *idle_frames[] = {
    (lv_image_dsc_t *)&p1_walk01,
    (lv_image_dsc_t *)&p1_walk02,
    (lv_image_dsc_t *)&p1_walk03,
    (lv_image_dsc_t *)&p1_walk04,
};

static lv_image_dsc_t *walk_frames[] = {
    (lv_image_dsc_t *)&p1_walk01,
    (lv_image_dsc_t *)&p1_walk02,
    (lv_image_dsc_t *)&p1_walk03,
    (lv_image_dsc_t *)&p1_walk04,
    (lv_image_dsc_t *)&p1_walk05,
    (lv_image_dsc_t *)&p1_walk06,
};

static lv_image_dsc_t *attack_frames[] = {
    (lv_image_dsc_t *)&p1_walk06,
    (lv_image_dsc_t *)&p1_walk07,
    (lv_image_dsc_t *)&p1_walk08,
};

static lv_image_dsc_t *hit_frames[] = {
    (lv_image_dsc_t *)&p1_walk01,
    (lv_image_dsc_t *)&p1_walk02,
};

static lv_image_dsc_t *death_frames[] = {
    (lv_image_dsc_t *)&p1_walk08,
    (lv_image_dsc_t *)&p1_walk09,
    (lv_image_dsc_t *)&p1_walk10,
};

static lv_image_dsc_t *levelup_frames[] = {
    (lv_image_dsc_t *)&p1_walk01,
    (lv_image_dsc_t *)&p1_walk02,
    (lv_image_dsc_t *)&p1_walk03,
};

static lv_image_dsc_t *slime_frames[] = {
    (lv_image_dsc_t *)&slimeWalk1,
    (lv_image_dsc_t *)&slimeWalk2,
};

static lv_image_dsc_t *fish_frames[] = {
    (lv_image_dsc_t *)&fishSwim1,
    (lv_image_dsc_t *)&fishSwim2,
};

static lv_image_dsc_t *fly_frames[] = {
    (lv_image_dsc_t *)&flyFly1,
    (lv_image_dsc_t *)&flyFly2,
};

static lv_image_dsc_t *blocker_frames[] = {
    (lv_image_dsc_t *)&blockerBody,
    (lv_image_dsc_t *)&blockerMad,
};

static lv_image_dsc_t *poker_frames[] = {
    (lv_image_dsc_t *)&pokerMad,
    (lv_image_dsc_t *)&pokerSad,
};

void sprites_init(void)
{
    ESP_LOGI(TAG, "Sprites initialized");
}

void sprites_set_idle_animation(lv_obj_t *animimg)
{
    if (!animimg) return;
    lv_anim_del(animimg, NULL);
    lv_animimg_set_src(animimg, (const void **)idle_frames, 4);
    lv_animimg_set_duration(animimg, 800);
    lv_animimg_set_repeat_count(animimg, LV_ANIM_REPEAT_INFINITE);
    lv_animimg_start(animimg);
}

void sprites_set_walk_animation(lv_obj_t *animimg)
{
    if (!animimg) return;
    lv_anim_del(animimg, NULL);
    lv_animimg_set_src(animimg, (const void **)walk_frames, 6);
    lv_animimg_set_duration(animimg, 600);
    lv_animimg_set_repeat_count(animimg, LV_ANIM_REPEAT_INFINITE);
    lv_animimg_start(animimg);
}

void sprites_set_attack_animation(lv_obj_t *animimg)
{
    if (!animimg) return;
    lv_anim_del(animimg, NULL);
    lv_animimg_set_src(animimg, (const void **)attack_frames, 3);
    lv_animimg_set_duration(animimg, 300);
    lv_animimg_set_repeat_count(animimg, 1);
    lv_animimg_start(animimg);
}

void sprites_set_hit_animation(lv_obj_t *animimg)
{
    if (!animimg) return;
    lv_anim_del(animimg, NULL);
    lv_animimg_set_src(animimg, (const void **)hit_frames, 2);
    lv_animimg_set_duration(animimg, 200);
    lv_animimg_set_repeat_count(animimg, 1);
    lv_animimg_start(animimg);
}

void sprites_set_death_animation(lv_obj_t *animimg)
{
    if (!animimg) return;
    lv_anim_del(animimg, NULL);
    lv_animimg_set_src(animimg, (const void **)death_frames, 3);
    lv_animimg_set_duration(animimg, 600);
    lv_animimg_set_repeat_count(animimg, 1);
    lv_animimg_start(animimg);
}

void sprites_set_levelup_animation(lv_obj_t *animimg)
{
    if (!animimg) return;
    lv_anim_del(animimg, NULL);
    lv_animimg_set_src(animimg, (const void **)levelup_frames, 3);
    lv_animimg_set_duration(animimg, 500);
    lv_animimg_set_repeat_count(animimg, 2);
    lv_animimg_start(animimg);
}

void sprites_set_enemy_slime_animation(lv_obj_t *animimg)
{
    if (!animimg) return;
    lv_anim_del(animimg, NULL);
    lv_animimg_set_src(animimg, (const void **)slime_frames, 2);
    lv_animimg_set_duration(animimg, 400);
    lv_animimg_set_repeat_count(animimg, LV_ANIM_REPEAT_INFINITE);
    lv_animimg_start(animimg);
}

void sprites_set_enemy_fish_animation(lv_obj_t *animimg)
{
    if (!animimg) return;
    lv_animimg_set_src(animimg, (const void **)fish_frames, 2);
    lv_animimg_set_duration(animimg, 400);
    lv_animimg_set_repeat_count(animimg, LV_ANIM_REPEAT_INFINITE);
    lv_animimg_start(animimg);
}

void sprites_set_enemy_fly_animation(lv_obj_t *animimg)
{
    if (!animimg) return;
    lv_animimg_set_src(animimg, (const void **)fly_frames, 2);
    lv_animimg_set_duration(animimg, 350);
    lv_animimg_set_repeat_count(animimg, LV_ANIM_REPEAT_INFINITE);
    lv_animimg_start(animimg);
}

void sprites_set_enemy_blocker_animation(lv_obj_t *animimg)
{
    if (!animimg) return;
    lv_animimg_set_src(animimg, (const void **)blocker_frames, 2);
    lv_animimg_set_duration(animimg, 600);
    lv_animimg_set_repeat_count(animimg, LV_ANIM_REPEAT_INFINITE);
    lv_animimg_start(animimg);
}

void sprites_set_enemy_poker_animation(lv_obj_t *animimg)
{
    if (!animimg) return;
    lv_animimg_set_src(animimg, (const void **)poker_frames, 2);
    lv_animimg_set_duration(animimg, 500);
    lv_animimg_set_repeat_count(animimg, LV_ANIM_REPEAT_INFINITE);
    lv_animimg_start(animimg);
}

void sprites_stop_animation(lv_obj_t *animimg)
{
    if (!animimg) return;
    lv_anim_del(animimg, NULL);
}

void sprites_create_arena_background(lv_obj_t *parent)
{
    if (!parent) return;
    lv_obj_set_style_bg_color(parent, lv_color_hex(COLOR_BLACK), 0);
}
