#include "screens.h"
#include "sprites.h"
#include "mistolito.h"
#include "ui_colors.h"
#include "storage_task.h"
#include "game_coordinator.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>
#include "ui_layouts/ui_layout_game.h"
#include "ui_layouts/ui_layout_rest.h"
#include "ui_layouts/ui_layout_init.h"
#include "ui_layouts/ui_layout_death.h"
#include "ui_layouts/ui_layout_searching.h"

LV_FONT_DECLARE(lv_font_montserrat_10);
LV_FONT_DECLARE(lv_font_montserrat_12);

static const char *TAG = "SCREENS";

static lv_obj_t *screens[5] = {NULL, NULL, NULL, NULL, NULL};
static screen_id_e current_screen = SCREEN_INIT;

static lv_obj_t *lbl_init_title = NULL;
static lv_obj_t *lbl_init_hint = NULL;
static lv_obj_t *lbl_init_status = NULL;

static lv_obj_t *arena_bg = NULL;
static lv_obj_t *pet_sprite = NULL;
static lv_obj_t *enemy_sprite = NULL;
static lv_obj_t *enemy_shadow = NULL;
static lv_obj_t *pet_shadow = NULL;
static lv_obj_t *enemy_damage_label = NULL;
static lv_obj_t *pet_exp_label = NULL;
static lv_obj_t *enemy_name_label = NULL;
static lv_obj_t *enemy_hp_bar = NULL;
static lv_obj_t *combat_log_label = NULL;

static lv_obj_t *stats_bg = NULL;
static lv_obj_t *pet_name_label = NULL;
static lv_obj_t *level_label = NULL;
static lv_obj_t *exp_label = NULL;
static lv_obj_t *energy_label = NULL;
static lv_obj_t *dp_label = NULL;
static lv_obj_t *hp_label = NULL;
static lv_obj_t *stats_row = NULL;
static lv_obj_t *pet_hp_bar = NULL;
static lv_obj_t *pet_en_bar = NULL;
static lv_obj_t *pet_xp_bar = NULL;
static lv_obj_t *stats_line = NULL;

static lv_style_t style_box;

static lv_obj_t *death_gameover_label = NULL;
static lv_obj_t *death_resurrect_label = NULL;

static lv_obj_t *rest_bg = NULL;
static lv_obj_t *rest_pet_sprite = NULL;
static lv_obj_t *rest_hp_popup = NULL;
static lv_obj_t *rest_en_popup = NULL;
static lv_obj_t *rest_stats_bg = NULL;
static lv_obj_t *rest_name_label = NULL;
static lv_obj_t *rest_level_label = NULL;
static lv_obj_t *rest_hp_label = NULL;
static lv_obj_t *rest_en_label = NULL;

static lv_obj_t *searching_arena_bg = NULL;
static lv_obj_t *searching_pet_sprite = NULL;
static lv_obj_t *searching_log_label = NULL;
static lv_obj_t *searching_stats_bg = NULL;
static lv_obj_t *searching_pet_name_label = NULL;
static lv_obj_t *searching_level_label = NULL;
static lv_obj_t *searching_dp_label = NULL;
static lv_obj_t *searching_hp_label = NULL;
static lv_obj_t *searching_hp_bar = NULL;
static lv_obj_t *searching_energy_label = NULL;
static lv_obj_t *searching_en_bar = NULL;
static lv_obj_t *searching_exp_label = NULL;
static lv_obj_t *searching_xp_bar = NULL;
static lv_obj_t *searching_stats_line = NULL;
static lv_obj_t *searching_stats_row = NULL;

static const char* state_str[] = {"INIT", "SEARCHING", "COMBAT", "VICTORY", "LEVELUP", "RESTING", "DEAD"};
static const char* prof_str[] = {"NOV", "WAR", "MAG", "ROG"};

#define UI_DIRTY_NAME       (1 << 0)
#define UI_DIRTY_LEVEL      (1 << 1)
#define UI_DIRTY_HP         (1 << 2)
#define UI_DIRTY_ENERGY     (1 << 3)
#define UI_DIRTY_DP         (1 << 4)
#define UI_DIRTY_STATS      (1 << 5)
#define UI_DIRTY_ENEMY_HP   (1 << 6)
#define UI_DIRTY_ENEMY_NAME (1 << 7)
#define UI_DIRTY_EXP        (1 << 8)
#define UI_DIRTY_PROFESSION (1 << 9)
#define UI_DIRTY_STATE      (1 << 10)

typedef struct {
    char name[PET_NAME_MAX_LEN];
    uint8_t level;
    int16_t hp;
    int16_t hp_max;
    uint8_t energy;
    uint8_t energy_max;
    uint32_t dp;
    uint32_t exp;
    uint32_t exp_next;
    uint32_t exp_prev;
    uint8_t stats[STAT_COUNT];
    uint8_t profession;
    game_state_e state;
    char enemy_name[ENEMY_NAME_MAX_LEN];
    int16_t enemy_hp;
    int16_t enemy_hp_max;
    bool enemy_visible;
    char buf_rest_name[PET_NAME_MAX_LEN];
    char buf_rest_level[16];
    char buf_rest_hp[16];
    char buf_rest_en[16];
    char buf_name[PET_NAME_MAX_LEN];
    char buf_level[16];
    char buf_hp[16];
    char buf_energy[16];
    char buf_dp[24];
    char buf_exp[16];
    char buf_profession[16];
    char buf_stats[64];
    char buf_combat_log[32];
    char buf_enemy_name[ENEMY_NAME_MAX_LEN];
} ui_cache_t;

static ui_cache_t ui_cache = {0};
static uint16_t ui_dirty_flags = 0;

static void init_styles(void)
{
    lv_style_init(&style_box);
    lv_style_set_bg_color(&style_box, lv_color_hex(COLOR_BLACK));
    lv_style_set_bg_opa(&style_box, LV_OPA_COVER);
    lv_style_set_border_width(&style_box, 1);
    lv_style_set_border_color(&style_box, lv_color_hex(COLOR_WHITE));
    lv_style_set_radius(&style_box, 0);
    lv_style_set_pad_all(&style_box, 2);
}

static void create_init_screen(void)
{
    screens[SCREEN_INIT] = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screens[SCREEN_INIT], lv_color_black(), 0);

    lbl_init_title = lv_label_create(screens[SCREEN_INIT]);
    lv_obj_set_style_text_color(lbl_init_title, lv_color_make(0x00, 0xFF, 0x00), 0);
    lv_obj_set_style_text_font(lbl_init_title, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(lbl_init_title, UI_INIT_TITLE_X, UI_INIT_TITLE_Y);
    lv_label_set_text(lbl_init_title, "MistolitoRPG v2.0");

    lbl_init_hint = lv_label_create(screens[SCREEN_INIT]);
    lv_obj_set_style_text_color(lbl_init_hint, lv_color_white(), 0);
    lv_obj_set_style_text_font(lbl_init_hint, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(lbl_init_hint, UI_INIT_HINT_X, UI_INIT_HINT_Y);
    lv_label_set_text(lbl_init_hint, "Press BOOT to start");

    lbl_init_status = lv_label_create(screens[SCREEN_INIT]);
    lv_obj_set_style_text_color(lbl_init_status, lv_color_make(0xAA, 0xAA, 0xAA), 0);
    lv_obj_set_style_text_font(lbl_init_status, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(lbl_init_status, UI_INIT_STATUS_X, UI_INIT_STATUS_Y);
    lv_label_set_text(lbl_init_status, "Waiting...");

    ESP_LOGI(TAG, "INIT screen created");
}

void screens_set_init_hint(const char *text)
{
    if (lbl_init_hint) {
        lv_label_set_text(lbl_init_hint, text);
    }
}

static void create_game_screen(void)
{
    screens[SCREEN_GAME] = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screens[SCREEN_GAME], lv_color_black(), 0);

    lv_obj_t *scr = screens[SCREEN_GAME];

    arena_bg = lv_obj_create(scr);
    lv_obj_set_size(arena_bg, UI_GAME_LCD_WIDTH, UI_GAME_ARENA_HEIGHT);
    lv_obj_set_pos(arena_bg, 0, UI_GAME_ARENA_Y);
    lv_obj_set_style_bg_color(arena_bg, lv_color_hex(COLOR_ARENA_GAME), 0);
    lv_obj_set_style_border_width(arena_bg, 0, 0);
    lv_obj_set_style_radius(arena_bg, 0, 0);
    lv_obj_clear_flag(arena_bg, LV_OBJ_FLAG_SCROLLABLE);

    sprites_create_arena_background(arena_bg);

    pet_shadow = lv_obj_create(arena_bg);
    lv_obj_set_size(pet_shadow, UI_GAME_PET_SHADOW_W, UI_GAME_PET_SHADOW_H);
    lv_obj_set_pos(pet_shadow, UI_GAME_PET_SHADOW_X, UI_GAME_PET_SHADOW_Y);
    lv_obj_set_style_bg_color(pet_shadow, lv_color_hex(COLOR_BLACK), 0);
    lv_obj_set_style_bg_opa(pet_shadow, LV_OPA_30, 0);
    lv_obj_set_style_border_width(pet_shadow, 0, 0);
    lv_obj_set_style_radius(pet_shadow, 5, 0);

    pet_sprite = lv_animimg_create(arena_bg);
    lv_obj_set_size(pet_sprite, UI_GAME_PET_W, UI_GAME_PET_H);
    lv_obj_set_pos(pet_sprite, UI_GAME_PET_X, UI_GAME_PET_Y);
    sprites_set_idle_animation(pet_sprite);

    enemy_shadow = lv_obj_create(arena_bg);
    lv_obj_set_size(enemy_shadow, UI_GAME_ENEMY_SHADOW_W, UI_GAME_ENEMY_SHADOW_H);
    lv_obj_set_pos(enemy_shadow, UI_GAME_ENEMY_SHADOW_X, UI_GAME_ENEMY_SHADOW_Y);
    lv_obj_set_style_bg_color(enemy_shadow, lv_color_hex(COLOR_BLACK), 0);
    lv_obj_set_style_bg_opa(enemy_shadow, LV_OPA_30, 0);
    lv_obj_set_style_border_width(enemy_shadow, 0, 0);
    lv_obj_set_style_radius(enemy_shadow, 5, 0);
    lv_obj_add_flag(enemy_shadow, LV_OBJ_FLAG_HIDDEN);

    enemy_sprite = lv_animimg_create(arena_bg);
    lv_obj_set_size(enemy_sprite, UI_GAME_ENEMY_W, UI_GAME_ENEMY_H);
    lv_obj_set_pos(enemy_sprite, UI_GAME_ENEMY_X, UI_GAME_ENEMY_Y);
    lv_obj_add_flag(enemy_sprite, LV_OBJ_FLAG_HIDDEN);

    enemy_damage_label = lv_label_create(arena_bg);
    lv_label_set_text(enemy_damage_label, "");
    lv_obj_set_style_text_color(enemy_damage_label, lv_color_hex(COLOR_DAMAGE_RED), 0);
    lv_obj_set_style_text_font(enemy_damage_label, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(enemy_damage_label, UI_GAME_ENEMY_DAMAGE_X, UI_GAME_ENEMY_DAMAGE_Y);
    lv_obj_add_flag(enemy_damage_label, LV_OBJ_FLAG_HIDDEN);

    pet_exp_label = lv_label_create(arena_bg);
    lv_label_set_text(pet_exp_label, "");
    lv_obj_set_style_text_color(pet_exp_label, lv_color_hex(COLOR_EXP_POPUP_YELLOW), 0);
    lv_obj_set_style_text_font(pet_exp_label, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(pet_exp_label, UI_GAME_PET_EXP_X, UI_GAME_PET_EXP_Y);
    lv_obj_add_flag(pet_exp_label, LV_OBJ_FLAG_HIDDEN);

    enemy_name_label = lv_label_create(arena_bg);
    lv_label_set_text(enemy_name_label, "---");
    lv_obj_set_style_text_color(enemy_name_label, lv_color_hex(COLOR_WHITE), 0);
    lv_obj_set_style_text_font(enemy_name_label, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(enemy_name_label, UI_GAME_ENEMY_NAME_X, UI_GAME_ENEMY_NAME_Y);

    enemy_hp_bar = lv_bar_create(arena_bg);
    lv_obj_set_size(enemy_hp_bar, UI_GAME_ENEMY_HP_BAR_W, UI_GAME_ENEMY_HP_BAR_H);
    lv_obj_set_pos(enemy_hp_bar, UI_GAME_ENEMY_HP_BAR_X, UI_GAME_ENEMY_HP_BAR_Y);
    lv_bar_set_range(enemy_hp_bar, 0, 100);
    lv_bar_set_value(enemy_hp_bar, 100, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(enemy_hp_bar, lv_color_hex(COLOR_BLACK), LV_PART_MAIN);
    lv_obj_set_style_bg_color(enemy_hp_bar, lv_color_hex(COLOR_WHITE), LV_PART_INDICATOR);

    combat_log_label = lv_label_create(arena_bg);
    lv_label_set_text(combat_log_label, "");
    lv_obj_set_style_text_color(combat_log_label, lv_color_hex(COLOR_WHITE), 0);
    lv_obj_set_style_text_font(combat_log_label, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(combat_log_label, UI_GAME_COMBAT_LOG_X, UI_GAME_COMBAT_LOG_Y);

    stats_bg = lv_obj_create(scr);
    lv_obj_set_size(stats_bg, UI_GAME_LCD_WIDTH, UI_GAME_STATS_HEIGHT);
    lv_obj_set_pos(stats_bg, 0, UI_GAME_STATS_Y);
    lv_obj_add_style(stats_bg, &style_box, 0);
    lv_obj_clear_flag(stats_bg, LV_OBJ_FLAG_SCROLLABLE);

    pet_name_label = lv_label_create(stats_bg);
    lv_label_set_text(pet_name_label, "Mistolito");
    lv_obj_set_style_text_color(pet_name_label, lv_color_hex(COLOR_WHITE), 0);
    lv_obj_set_style_text_font(pet_name_label, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(pet_name_label, UI_GAME_PET_NAME_X, UI_GAME_PET_NAME_Y);

    level_label = lv_label_create(stats_bg);
    lv_label_set_text(level_label, "Lv1");
    lv_obj_set_style_text_color(level_label, lv_color_hex(COLOR_LIGHT_GRAY), 0);
    lv_obj_set_style_text_font(level_label, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(level_label, UI_GAME_PET_LEVEL_X, UI_GAME_PET_LEVEL_Y);

    dp_label = lv_label_create(stats_bg);
    lv_label_set_text(dp_label, "DP0");
    lv_obj_set_style_text_color(dp_label, lv_color_hex(COLOR_DP_GOLD), 0);
    lv_obj_set_style_text_font(dp_label, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(dp_label, UI_GAME_PET_DP_X, UI_GAME_PET_DP_Y);

    hp_label = lv_label_create(stats_bg);
    lv_label_set_text(hp_label, "HP 100/100");
    lv_obj_set_style_text_color(hp_label, lv_color_hex(COLOR_WHITE), 0);
    lv_obj_set_style_text_font(hp_label, &lv_font_montserrat_10, 0);
    lv_obj_set_pos(hp_label, UI_GAME_BARS_X, UI_GAME_HP_TEXT_Y);

    pet_hp_bar = lv_bar_create(stats_bg);
    lv_obj_set_size(pet_hp_bar, UI_GAME_BARS_WIDTH, UI_GAME_HP_BAR_H);
    lv_obj_set_pos(pet_hp_bar, UI_GAME_BARS_X, UI_GAME_HP_BAR_Y);
    lv_bar_set_range(pet_hp_bar, 0, 100);
    lv_bar_set_value(pet_hp_bar, 100, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(pet_hp_bar, lv_color_hex(COLOR_DARK_GRAY), LV_PART_MAIN);
    lv_obj_set_style_bg_color(pet_hp_bar, lv_color_hex(COLOR_HP_GREEN), LV_PART_INDICATOR);

    energy_label = lv_label_create(stats_bg);
    lv_label_set_text(energy_label, "EN 10/10");
    lv_obj_set_style_text_color(energy_label, lv_color_hex(COLOR_WHITE), 0);
    lv_obj_set_style_text_font(energy_label, &lv_font_montserrat_10, 0);
    lv_obj_set_pos(energy_label, UI_GAME_BARS_X, UI_GAME_EN_TEXT_Y);

    pet_en_bar = lv_bar_create(stats_bg);
    lv_obj_set_size(pet_en_bar, UI_GAME_BARS_WIDTH, UI_GAME_EN_BAR_H);
    lv_obj_set_pos(pet_en_bar, UI_GAME_BARS_X, UI_GAME_EN_BAR_Y);
    lv_bar_set_range(pet_en_bar, 0, 100);
    lv_bar_set_value(pet_en_bar, 100, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(pet_en_bar, lv_color_hex(COLOR_DARK_GRAY), LV_PART_MAIN);
    lv_obj_set_style_bg_color(pet_en_bar, lv_color_hex(COLOR_EN_BLUE), LV_PART_INDICATOR);

    exp_label = lv_label_create(stats_bg);
    lv_label_set_text(exp_label, "XP 0%");
    lv_obj_set_style_text_color(exp_label, lv_color_hex(COLOR_WHITE), 0);
    lv_obj_set_style_text_font(exp_label, &lv_font_montserrat_10, 0);
    lv_obj_set_pos(exp_label, UI_GAME_BARS_X, UI_GAME_XP_TEXT_Y);

    pet_xp_bar = lv_bar_create(stats_bg);
    lv_obj_set_size(pet_xp_bar, UI_GAME_BARS_WIDTH, UI_GAME_XP_BAR_H);
    lv_obj_set_pos(pet_xp_bar, UI_GAME_BARS_X, UI_GAME_XP_BAR_Y);
    lv_bar_set_range(pet_xp_bar, 0, 100);
    lv_bar_set_value(pet_xp_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(pet_xp_bar, lv_color_hex(COLOR_DARK_GRAY), LV_PART_MAIN);
    lv_obj_set_style_bg_color(pet_xp_bar, lv_color_hex(COLOR_XP_YELLOW), LV_PART_INDICATOR);

    stats_line = lv_line_create(stats_bg);
    static lv_point_precise_t line_points[2];
    line_points[0].x = 4;
    line_points[0].y = UI_GAME_STATS_LINE_Y;
    line_points[1].x = 316;
    line_points[1].y = UI_GAME_STATS_LINE_Y;
    lv_line_set_points(stats_line, line_points, 2);
    lv_obj_set_style_line_color(stats_line, lv_color_hex(COLOR_DARK_GRAY), 0);
    lv_obj_set_style_line_width(stats_line, 1, 0);

    stats_row = lv_label_create(stats_bg);
    lv_label_set_text(stats_row, "STR:10 DEX:10 CON:10 INT:10 WIS:10 CHA:10");
    lv_obj_set_style_text_color(stats_row, lv_color_hex(COLOR_STATS_TEXT_CYAN), 0);
    lv_obj_set_style_text_font(stats_row, &lv_font_montserrat_10, 0);
    lv_obj_set_pos(stats_row, UI_GAME_STATS_TEXT_X, UI_GAME_STATS_TEXT_Y);

    ESP_LOGI(TAG, "GAME screen created");
}

static void create_death_screen(void)
{
    screens[SCREEN_DEATH] = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screens[SCREEN_DEATH], lv_color_black(), 0);

    death_gameover_label = lv_label_create(screens[SCREEN_DEATH]);
    lv_obj_set_style_text_color(death_gameover_label, lv_color_hex(COLOR_DAMAGE_RED), 0);
    lv_obj_set_style_text_font(death_gameover_label, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(death_gameover_label, UI_DEATH_GAMEOVER_X, UI_DEATH_GAMEOVER_Y);
    lv_label_set_text(death_gameover_label, "GAME OVER");

    death_resurrect_label = lv_label_create(screens[SCREEN_DEATH]);
    lv_obj_set_style_text_color(death_resurrect_label, lv_color_hex(COLOR_GRAY_TEXT), 0);
    lv_obj_set_style_text_font(death_resurrect_label, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(death_resurrect_label, UI_DEATH_RESURRECT_X, UI_DEATH_RESURRECT_Y);
    lv_label_set_text(death_resurrect_label, "(resucitando...)");

    ESP_LOGI(TAG, "DEATH screen created");
}

static void create_rest_screen(void)
{
    screens[SCREEN_REST] = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screens[SCREEN_REST], lv_color_hex(COLOR_ARENA_REST), 0);

    lv_obj_t *scr = screens[SCREEN_REST];

    rest_bg = lv_obj_create(scr);
    lv_obj_set_size(rest_bg, UI_REST_LCD_WIDTH, UI_REST_STATS_Y);
    lv_obj_set_pos(rest_bg, 0, 0);
    lv_obj_set_style_bg_color(rest_bg, lv_color_hex(COLOR_ARENA_REST), 0);
    lv_obj_set_style_border_width(rest_bg, 0, 0);
    lv_obj_set_style_radius(rest_bg, 0, 0);
    lv_obj_clear_flag(rest_bg, LV_OBJ_FLAG_SCROLLABLE);

    rest_pet_sprite = lv_animimg_create(rest_bg);
    lv_obj_set_size(rest_pet_sprite, UI_REST_PET_W, UI_REST_PET_H);
    lv_obj_set_pos(rest_pet_sprite, UI_REST_PET_X, UI_REST_PET_Y);
    sprites_set_idle_animation(rest_pet_sprite);

    rest_hp_popup = lv_label_create(rest_bg);
    lv_label_set_text(rest_hp_popup, "");
    lv_obj_set_style_text_color(rest_hp_popup, lv_color_hex(COLOR_HP_GREEN), 0);
    lv_obj_set_style_text_font(rest_hp_popup, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(rest_hp_popup, UI_REST_HP_POPUP_X, UI_REST_HP_POPUP_Y);
    lv_obj_add_flag(rest_hp_popup, LV_OBJ_FLAG_HIDDEN);

    rest_en_popup = lv_label_create(rest_bg);
    lv_label_set_text(rest_en_popup, "");
    lv_obj_set_style_text_color(rest_en_popup, lv_color_hex(COLOR_EN_BLUE), 0);
    lv_obj_set_style_text_font(rest_en_popup, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(rest_en_popup, UI_REST_EN_POPUP_X, UI_REST_EN_POPUP_Y);
    lv_obj_add_flag(rest_en_popup, LV_OBJ_FLAG_HIDDEN);

    rest_stats_bg = lv_obj_create(scr);
    lv_obj_set_size(rest_stats_bg, UI_REST_LCD_WIDTH, UI_REST_STATS_HEIGHT);
    lv_obj_set_pos(rest_stats_bg, 0, UI_REST_STATS_Y);
    lv_obj_add_style(rest_stats_bg, &style_box, 0);
    lv_obj_clear_flag(rest_stats_bg, LV_OBJ_FLAG_SCROLLABLE);

    rest_name_label = lv_label_create(rest_stats_bg);
    lv_label_set_text(rest_name_label, "Mistolito");
    lv_obj_set_style_text_color(rest_name_label, lv_color_hex(COLOR_WHITE), 0);
    lv_obj_set_style_text_font(rest_name_label, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(rest_name_label, UI_REST_NAME_X, UI_REST_NAME_Y);

    rest_level_label = lv_label_create(rest_stats_bg);
    lv_label_set_text(rest_level_label, "Lv1");
    lv_obj_set_style_text_color(rest_level_label, lv_color_hex(COLOR_WHITE), 0);
    lv_obj_set_style_text_font(rest_level_label, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(rest_level_label, UI_REST_LEVEL_X, UI_REST_LEVEL_Y);

    rest_hp_label = lv_label_create(rest_stats_bg);
    lv_label_set_text(rest_hp_label, "HP100%");
    lv_obj_set_style_text_color(rest_hp_label, lv_color_hex(COLOR_WHITE), 0);
    lv_obj_set_style_text_font(rest_hp_label, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(rest_hp_label, UI_REST_HP_X, UI_REST_HP_Y);

    rest_en_label = lv_label_create(rest_stats_bg);
    lv_label_set_text(rest_en_label, "EN100%");
    lv_obj_set_style_text_color(rest_en_label, lv_color_hex(COLOR_WHITE), 0);
    lv_obj_set_style_text_font(rest_en_label, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(rest_en_label, UI_REST_EN_X, UI_REST_EN_Y);

    ESP_LOGI(TAG, "REST screen created");
}

static void create_searching_screen(void)
{
    screens[SCREEN_SEARCHING] = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screens[SCREEN_SEARCHING], lv_color_black(), 0);

    lv_obj_t *scr = screens[SCREEN_SEARCHING];

    searching_arena_bg = lv_obj_create(scr);
    lv_obj_set_size(searching_arena_bg, UI_SEARCH_LCD_WIDTH, UI_SEARCH_ARENA_HEIGHT);
    lv_obj_set_pos(searching_arena_bg, 0, UI_SEARCH_ARENA_Y);
    lv_obj_set_style_bg_color(searching_arena_bg, lv_color_hex(COLOR_BG_FAINT_BLUE), 0);
    lv_obj_set_style_border_width(searching_arena_bg, 0, 0);
    lv_obj_set_style_radius(searching_arena_bg, 0, 0);
    lv_obj_clear_flag(searching_arena_bg, LV_OBJ_FLAG_SCROLLABLE);

    for (int i = 0; i < UI_SEARCH_FLOOR_TILE_COUNT; ++i)
    {
        lv_obj_t *tile = lv_image_create(searching_arena_bg);
        lv_image_set_src(tile, &grassMid);
        lv_obj_set_pos(tile, i * UI_SEARCH_FLOOR_TILE_W, UI_SEARCH_FLOOR_Y);
    }

    searching_pet_sprite = lv_animimg_create(searching_arena_bg);
    lv_obj_set_size(searching_pet_sprite, UI_SEARCH_PET_W, UI_SEARCH_PET_H);
    lv_obj_set_pos(searching_pet_sprite, UI_SEARCH_PET_X, UI_SEARCH_PET_Y);
    sprites_set_walk_animation(searching_pet_sprite);

    searching_log_label = lv_label_create(searching_arena_bg);
    lv_label_set_text(searching_log_label, "Buscando enemigos...");
    lv_obj_set_style_text_color(searching_log_label, lv_color_hex(COLOR_WHITE), 0);
    lv_obj_set_style_text_font(searching_log_label, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(searching_log_label, UI_SEARCH_LOG_X, UI_SEARCH_LOG_Y);

    searching_stats_bg = lv_obj_create(scr);
    lv_obj_set_size(searching_stats_bg, UI_SEARCH_LCD_WIDTH, UI_SEARCH_STATS_HEIGHT);
    lv_obj_set_pos(searching_stats_bg, 0, UI_SEARCH_STATS_Y);
    lv_obj_add_style(searching_stats_bg, &style_box, 0);
    lv_obj_clear_flag(searching_stats_bg, LV_OBJ_FLAG_SCROLLABLE);

    searching_pet_name_label = lv_label_create(searching_stats_bg);
    lv_label_set_text(searching_pet_name_label, "Mistolito");
    lv_obj_set_style_text_color(searching_pet_name_label, lv_color_hex(COLOR_WHITE), 0);
    lv_obj_set_style_text_font(searching_pet_name_label, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(searching_pet_name_label, UI_SEARCH_PET_NAME_X, UI_SEARCH_PET_NAME_Y);

    searching_level_label = lv_label_create(searching_stats_bg);
    lv_label_set_text(searching_level_label, "Lv1");
    lv_obj_set_style_text_color(searching_level_label, lv_color_hex(COLOR_LIGHT_GRAY), 0);
    lv_obj_set_style_text_font(searching_level_label, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(searching_level_label, UI_SEARCH_PET_LEVEL_X, UI_SEARCH_PET_LEVEL_Y);

    searching_dp_label = lv_label_create(searching_stats_bg);
    lv_label_set_text(searching_dp_label, "DP0");
    lv_obj_set_style_text_color(searching_dp_label, lv_color_hex(COLOR_DP_GOLD), 0);
    lv_obj_set_style_text_font(searching_dp_label, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(searching_dp_label, UI_SEARCH_PET_DP_X, UI_SEARCH_PET_DP_Y);

    searching_hp_label = lv_label_create(searching_stats_bg);
    lv_label_set_text(searching_hp_label, "HP 100/100");
    lv_obj_set_style_text_color(searching_hp_label, lv_color_hex(COLOR_WHITE), 0);
    lv_obj_set_style_text_font(searching_hp_label, &lv_font_montserrat_10, 0);
    lv_obj_set_pos(searching_hp_label, UI_SEARCH_BARS_X, UI_SEARCH_HP_TEXT_Y);

    searching_hp_bar = lv_bar_create(searching_stats_bg);
    lv_obj_set_size(searching_hp_bar, UI_SEARCH_BARS_WIDTH, UI_SEARCH_HP_BAR_H);
    lv_obj_set_pos(searching_hp_bar, UI_SEARCH_BARS_X, UI_SEARCH_HP_BAR_Y);
    lv_bar_set_range(searching_hp_bar, 0, 100);
    lv_bar_set_value(searching_hp_bar, 100, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(searching_hp_bar, lv_color_hex(COLOR_DARK_GRAY), LV_PART_MAIN);
    lv_obj_set_style_bg_color(searching_hp_bar, lv_color_hex(COLOR_HP_GREEN), LV_PART_INDICATOR);

    searching_energy_label = lv_label_create(searching_stats_bg);
    lv_label_set_text(searching_energy_label, "EN 10/10");
    lv_obj_set_style_text_color(searching_energy_label, lv_color_hex(COLOR_WHITE), 0);
    lv_obj_set_style_text_font(searching_energy_label, &lv_font_montserrat_10, 0);
    lv_obj_set_pos(searching_energy_label, UI_SEARCH_BARS_X, UI_SEARCH_EN_TEXT_Y);

    searching_en_bar = lv_bar_create(searching_stats_bg);
    lv_obj_set_size(searching_en_bar, UI_SEARCH_BARS_WIDTH, UI_SEARCH_EN_BAR_H);
    lv_obj_set_pos(searching_en_bar, UI_SEARCH_BARS_X, UI_SEARCH_EN_BAR_Y);
    lv_bar_set_range(searching_en_bar, 0, 100);
    lv_bar_set_value(searching_en_bar, 100, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(searching_en_bar, lv_color_hex(COLOR_DARK_GRAY), LV_PART_MAIN);
    lv_obj_set_style_bg_color(searching_en_bar, lv_color_hex(COLOR_EN_BLUE), LV_PART_INDICATOR);

    searching_exp_label = lv_label_create(searching_stats_bg);
    lv_label_set_text(searching_exp_label, "XP 0%");
    lv_obj_set_style_text_color(searching_exp_label, lv_color_hex(COLOR_WHITE), 0);
    lv_obj_set_style_text_font(searching_exp_label, &lv_font_montserrat_10, 0);
    lv_obj_set_pos(searching_exp_label, UI_SEARCH_BARS_X, UI_SEARCH_XP_TEXT_Y);

    searching_xp_bar = lv_bar_create(searching_stats_bg);
    lv_obj_set_size(searching_xp_bar, UI_SEARCH_BARS_WIDTH, UI_SEARCH_XP_BAR_H);
    lv_obj_set_pos(searching_xp_bar, UI_SEARCH_BARS_X, UI_SEARCH_XP_BAR_Y);
    lv_bar_set_range(searching_xp_bar, 0, 100);
    lv_bar_set_value(searching_xp_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(searching_xp_bar, lv_color_hex(COLOR_DARK_GRAY), LV_PART_MAIN);
    lv_obj_set_style_bg_color(searching_xp_bar, lv_color_hex(COLOR_XP_YELLOW), LV_PART_INDICATOR);

    searching_stats_line = lv_line_create(searching_stats_bg);
    static lv_point_precise_t line_points[2];
    line_points[0].x = 4;
    line_points[0].y = UI_SEARCH_STATS_LINE_Y;
    line_points[1].x = 316;
    line_points[1].y = UI_SEARCH_STATS_LINE_Y;
    lv_line_set_points(searching_stats_line, line_points, 2);
    lv_obj_set_style_line_color(searching_stats_line, lv_color_hex(COLOR_DARK_GRAY), 0);
    lv_obj_set_style_line_width(searching_stats_line, 1, 0);

    searching_stats_row = lv_label_create(searching_stats_bg);
    lv_label_set_text(searching_stats_row, "STR:10 DEX:10 CON:10 INT:10 WIS:10 CHA:10");
    lv_obj_set_style_text_color(searching_stats_row, lv_color_hex(COLOR_STATS_TEXT_CYAN), 0);
    lv_obj_set_style_text_font(searching_stats_row, &lv_font_montserrat_10, 0);
    lv_obj_set_pos(searching_stats_row, UI_SEARCH_STATS_TEXT_X, UI_SEARCH_STATS_TEXT_Y);

    ESP_LOGI(TAG, "SEARCHING screen created");
}

void screens_init(void)
{
    init_styles();
    sprites_init();
    create_init_screen();
    create_game_screen();
    create_searching_screen();
    create_rest_screen();
    create_death_screen();
    ESP_LOGI(TAG, "All screens initialized");
}

void screens_load(screen_id_e id)
{
    if (id >= SCREEN_COUNT || screens[id] == NULL) {
        ESP_LOGE(TAG, "Invalid screen id: %d", id);
        return;
    }

    lv_scr_load(screens[id]);
    current_screen = id;
    ESP_LOGI(TAG, "Loaded screen: %d", id);
}

static int get_enemy_sprite_type(const char *name)
{
    if (name == NULL) {
        return 0;
    }
    if (strstr(name, "slime") || strstr(name, "Slime")) {
        return 0;
    }
    if (strstr(name, "fish") || strstr(name, "Fish")) {
        return 1;
    }
    if (strstr(name, "fly") || strstr(name, "Fly")) {
        return 2;
    }
    if (strstr(name, "blocker") || strstr(name, "Blocker")) {
        return 3;
    }
    if (strstr(name, "poker") || strstr(name, "Poker")) {
        return 4;
    }
    return 0;
}

void screens_update(game_snapshot_t *snap)
{
    if (current_screen == SCREEN_INIT || snap == NULL) {
        return;
    }

    if (ui_cache.state != snap->state) {
        ui_cache.state = snap->state;
        ui_dirty_flags |= UI_DIRTY_STATE;

        switch (snap->state) {
        case GS_DEAD:
            screens_load(SCREEN_DEATH);
            break;
        case GS_RESTING:
            screens_load(SCREEN_REST);
            break;
        case GS_TRAINING:
            screens_load(SCREEN_GAME);
            break;
        case GS_COMBAT:
        case GS_VICTORY:
            if (current_screen != SCREEN_GAME) {
                screens_load(SCREEN_GAME);
            }
            break;
        case GS_SEARCHING:
            if (current_screen != SCREEN_SEARCHING) {
                screens_load(SCREEN_SEARCHING);
            }
            break;
        default:
            break;
        }
    }

    if (current_screen == SCREEN_REST) {
        if (strcmp(ui_cache.name, snap->pet.name) != 0) {
            strncpy(ui_cache.name, snap->pet.name, PET_NAME_MAX_LEN - 1);
            ui_cache.name[PET_NAME_MAX_LEN - 1] = '\0';
            ui_dirty_flags |= UI_DIRTY_NAME;
        }
        if (ui_cache.level != snap->pet.level) {
            ui_cache.level = snap->pet.level;
            ui_dirty_flags |= UI_DIRTY_LEVEL;
        }
        if (ui_cache.hp != snap->pet.hp || ui_cache.hp_max != snap->pet.hp_max) {
            ui_cache.hp = snap->pet.hp;
            ui_cache.hp_max = snap->pet.hp_max;
            ui_dirty_flags |= UI_DIRTY_HP;
        }
        if (ui_cache.energy != snap->pet.energy || ui_cache.energy_max != snap->pet.energy_max) {
            ui_cache.energy = snap->pet.energy;
            ui_cache.energy_max = snap->pet.energy_max;
            ui_dirty_flags |= UI_DIRTY_ENERGY;
        }

        if (ui_dirty_flags == 0) {
            return;
        }

        if (ui_dirty_flags & UI_DIRTY_NAME) {
            strncpy(ui_cache.buf_rest_name, ui_cache.name, sizeof(ui_cache.buf_rest_name) - 1);
            ui_cache.buf_rest_name[sizeof(ui_cache.buf_rest_name) - 1] = '\0';
            lv_label_set_text_static(rest_name_label, ui_cache.buf_rest_name);
            ui_dirty_flags &= ~UI_DIRTY_NAME;
        }
        if (ui_dirty_flags & UI_DIRTY_LEVEL) {
            lv_snprintf(ui_cache.buf_rest_level, sizeof(ui_cache.buf_rest_level), "Lv%d", ui_cache.level);
            lv_label_set_text_static(rest_level_label, ui_cache.buf_rest_level);
            ui_dirty_flags &= ~UI_DIRTY_LEVEL;
        }
        if (ui_dirty_flags & UI_DIRTY_HP) {
            uint8_t hp_pct = ui_cache.hp_max > 0 ? (ui_cache.hp * 100) / ui_cache.hp_max : 0;
            lv_snprintf(ui_cache.buf_rest_hp, sizeof(ui_cache.buf_rest_hp), "HP%d%%", hp_pct);
            lv_label_set_text_static(rest_hp_label, ui_cache.buf_rest_hp);
            ui_dirty_flags &= ~UI_DIRTY_HP;
        }
        if (ui_dirty_flags & UI_DIRTY_ENERGY) {
            uint8_t en_pct = ui_cache.energy_max > 0 ? (ui_cache.energy * 100) / ui_cache.energy_max : 0;
            lv_snprintf(ui_cache.buf_rest_en, sizeof(ui_cache.buf_rest_en), "EN%d%%", en_pct);
            lv_label_set_text_static(rest_en_label, ui_cache.buf_rest_en);
            ui_dirty_flags &= ~UI_DIRTY_ENERGY;
        }
        return;
    }

    if (current_screen == SCREEN_DEATH) {
        return;
    }

    if (strcmp(ui_cache.name, snap->pet.name) != 0) {
        strncpy(ui_cache.name, snap->pet.name, PET_NAME_MAX_LEN - 1);
        ui_cache.name[PET_NAME_MAX_LEN - 1] = '\0';
        ui_dirty_flags |= UI_DIRTY_NAME;
    }

    if (ui_cache.level != snap->pet.level) {
        ui_cache.level = snap->pet.level;
        ui_dirty_flags |= UI_DIRTY_LEVEL;
    }

    if (ui_cache.hp != snap->pet.hp || ui_cache.hp_max != snap->pet.hp_max) {
        ui_cache.hp = snap->pet.hp;
        ui_cache.hp_max = snap->pet.hp_max;
        ui_dirty_flags |= UI_DIRTY_HP;
    }

    if (ui_cache.energy != snap->pet.energy || ui_cache.energy_max != snap->pet.energy_max) {
        ui_cache.energy = snap->pet.energy;
        ui_cache.energy_max = snap->pet.energy_max;
        ui_dirty_flags |= UI_DIRTY_ENERGY;
    }

    if (ui_cache.dp != snap->pet.dp) {
        ui_cache.dp = snap->pet.dp;
        ui_dirty_flags |= UI_DIRTY_DP;
    }

    uint32_t total_trans = storage_replay_get_total();
    uint32_t trans_prev_level = game_coordinator_get_trans_for_level(snap->pet.level - 1);
    uint32_t trans_for_next = game_coordinator_get_trans_for_level(snap->pet.level);

    if (ui_cache.exp != total_trans || ui_cache.exp_next != trans_for_next) {
        ui_cache.exp = total_trans;
        ui_cache.exp_next = trans_for_next;
        ui_cache.exp_prev = trans_prev_level;
        ui_dirty_flags |= UI_DIRTY_EXP;
    }

    if (ui_cache.profession != snap->pet.profession) {
        ui_cache.profession = snap->pet.profession;
        ui_dirty_flags |= UI_DIRTY_PROFESSION;
    }

    if (ui_cache.stats[0] != snap->pet.str || ui_cache.stats[1] != snap->pet.dex ||
        ui_cache.stats[2] != snap->pet.con || ui_cache.stats[3] != snap->pet.intel ||
        ui_cache.stats[4] != snap->pet.wis || ui_cache.stats[5] != snap->pet.cha) {
        ui_cache.stats[0] = snap->pet.str;
        ui_cache.stats[1] = snap->pet.dex;
        ui_cache.stats[2] = snap->pet.con;
        ui_cache.stats[3] = snap->pet.intel;
        ui_cache.stats[4] = snap->pet.wis;
        ui_cache.stats[5] = snap->pet.cha;
        ui_dirty_flags |= UI_DIRTY_STATS;
    }

    bool enemy_visible = (snap->encounter.count > 0 && snap->encounter.enemies[0].alive);
    if (ui_cache.enemy_visible != enemy_visible) {
        ui_cache.enemy_visible = enemy_visible;
        ui_dirty_flags |= UI_DIRTY_ENEMY_HP | UI_DIRTY_ENEMY_NAME;
    }

    if (enemy_visible) {
        enemy_t *enemy = &snap->encounter.enemies[0];
        if (strcmp(ui_cache.enemy_name, enemy->name) != 0) {
            strncpy(ui_cache.enemy_name, enemy->name, ENEMY_NAME_MAX_LEN - 1);
            ui_cache.enemy_name[ENEMY_NAME_MAX_LEN - 1] = '\0';
            ui_dirty_flags |= UI_DIRTY_ENEMY_NAME;
        }
        if (ui_cache.enemy_hp != enemy->hp || ui_cache.enemy_hp_max != enemy->hp_max) {
            ui_cache.enemy_hp = enemy->hp;
            ui_cache.enemy_hp_max = enemy->hp_max;
            ui_dirty_flags |= UI_DIRTY_ENEMY_HP;
        }
    }

    if (ui_dirty_flags == 0) {
        return;
    }

    if (current_screen == SCREEN_SEARCHING) {
        if (ui_dirty_flags & UI_DIRTY_NAME) {
            strncpy(ui_cache.buf_name, ui_cache.name, sizeof(ui_cache.buf_name) - 1);
            ui_cache.buf_name[sizeof(ui_cache.buf_name) - 1] = '\0';
            lv_label_set_text_static(searching_pet_name_label, ui_cache.buf_name);
        }
        if (ui_dirty_flags & (UI_DIRTY_LEVEL | UI_DIRTY_PROFESSION)) {
            const char *prof_name = (ui_cache.profession < 4) ? prof_str[ui_cache.profession] : "???";
            lv_snprintf(ui_cache.buf_level, sizeof(ui_cache.buf_level), "Lv. %d %s", ui_cache.level, prof_name);
            lv_label_set_text_static(searching_level_label, ui_cache.buf_level);
        }
        if (ui_dirty_flags & UI_DIRTY_HP) {
            uint8_t hp_pct = ui_cache.hp_max > 0 ? (ui_cache.hp * 100) / ui_cache.hp_max : 0;
            lv_snprintf(ui_cache.buf_hp, sizeof(ui_cache.buf_hp), "HP %d/%d", ui_cache.hp, ui_cache.hp_max);
            lv_label_set_text_static(searching_hp_label, ui_cache.buf_hp);
            lv_bar_set_value(searching_hp_bar, hp_pct, LV_ANIM_ON);
        }
        if (ui_dirty_flags & UI_DIRTY_ENERGY) {
            uint8_t en_pct = ui_cache.energy_max > 0 ? (ui_cache.energy * 100) / ui_cache.energy_max : 0;
            lv_snprintf(ui_cache.buf_energy, sizeof(ui_cache.buf_energy), "EN %d/%d", ui_cache.energy, ui_cache.energy_max);
            lv_label_set_text_static(searching_energy_label, ui_cache.buf_energy);
            lv_bar_set_value(searching_en_bar, en_pct, LV_ANIM_ON);
        }
        if (ui_dirty_flags & UI_DIRTY_DP) {
            lv_snprintf(ui_cache.buf_dp, sizeof(ui_cache.buf_dp), "DP: %lu", (unsigned long)ui_cache.dp);
            lv_label_set_text_static(searching_dp_label, ui_cache.buf_dp);
        }
        if (ui_dirty_flags & UI_DIRTY_EXP) {
            uint32_t exp_in_lvl = ui_cache.exp > ui_cache.exp_prev ? ui_cache.exp - ui_cache.exp_prev : 0;
            uint32_t exp_per_lvl = ui_cache.exp_next > ui_cache.exp_prev ? ui_cache.exp_next - ui_cache.exp_prev : 1;
            uint8_t xp_pct = (uint8_t)((exp_in_lvl * 100) / exp_per_lvl);
            if (xp_pct > 100) xp_pct = 100;
            lv_snprintf(ui_cache.buf_exp, sizeof(ui_cache.buf_exp), "XP %d%%", xp_pct);
            lv_label_set_text_static(searching_exp_label, ui_cache.buf_exp);
            lv_bar_set_value(searching_xp_bar, xp_pct, LV_ANIM_ON);
        }
        if (ui_dirty_flags & UI_DIRTY_STATS) {
            lv_snprintf(ui_cache.buf_stats, sizeof(ui_cache.buf_stats), "STR:%d DEX:%d CON:%d INT:%d WIS:%d CHA:%d",
                ui_cache.stats[0], ui_cache.stats[1], ui_cache.stats[2],
                ui_cache.stats[3], ui_cache.stats[4], ui_cache.stats[5]);
            lv_label_set_text_static(searching_stats_row, ui_cache.buf_stats);
        }
        if (ui_dirty_flags & UI_DIRTY_STATE) {
            if (ui_cache.state < 7) {
                strncpy(ui_cache.buf_combat_log, state_str[ui_cache.state], sizeof(ui_cache.buf_combat_log) - 1);
                ui_cache.buf_combat_log[sizeof(ui_cache.buf_combat_log) - 1] = '\0';
                lv_label_set_text_static(searching_log_label, ui_cache.buf_combat_log);
            }
        }
        ui_dirty_flags = 0;
        return;
    }

    if (ui_dirty_flags & UI_DIRTY_NAME) {
        strncpy(ui_cache.buf_name, ui_cache.name, sizeof(ui_cache.buf_name) - 1);
        ui_cache.buf_name[sizeof(ui_cache.buf_name) - 1] = '\0';
        lv_label_set_text_static(pet_name_label, ui_cache.buf_name);
        ui_dirty_flags &= ~UI_DIRTY_NAME;
    }

    if (ui_dirty_flags & (UI_DIRTY_LEVEL | UI_DIRTY_PROFESSION)) {
        const char *prof_name = (ui_cache.profession < 4) ? prof_str[ui_cache.profession] : "???";
        lv_snprintf(ui_cache.buf_level, sizeof(ui_cache.buf_level), "Lv. %d %s", ui_cache.level, prof_name);
        lv_label_set_text_static(level_label, ui_cache.buf_level);
        ui_dirty_flags &= ~(UI_DIRTY_LEVEL | UI_DIRTY_PROFESSION);
    }

    if (ui_dirty_flags & UI_DIRTY_HP) {
        uint8_t hp_pct = ui_cache.hp_max > 0 ? (ui_cache.hp * 100) / ui_cache.hp_max : 0;
        lv_snprintf(ui_cache.buf_hp, sizeof(ui_cache.buf_hp), "HP %d/%d", ui_cache.hp, ui_cache.hp_max);
        lv_label_set_text_static(hp_label, ui_cache.buf_hp);
        lv_bar_set_value(pet_hp_bar, hp_pct, LV_ANIM_ON);
        ui_dirty_flags &= ~UI_DIRTY_HP;
    }

    if (ui_dirty_flags & UI_DIRTY_ENERGY) {
        uint8_t en_pct = ui_cache.energy_max > 0 ? (ui_cache.energy * 100) / ui_cache.energy_max : 0;
        lv_snprintf(ui_cache.buf_energy, sizeof(ui_cache.buf_energy), "EN %d/%d", ui_cache.energy, ui_cache.energy_max);
        lv_label_set_text_static(energy_label, ui_cache.buf_energy);
        lv_bar_set_value(pet_en_bar, en_pct, LV_ANIM_ON);
        ui_dirty_flags &= ~UI_DIRTY_ENERGY;
    }

    if (ui_dirty_flags & UI_DIRTY_DP) {
        lv_snprintf(ui_cache.buf_dp, sizeof(ui_cache.buf_dp), "DP: %lu", (unsigned long)ui_cache.dp);
        lv_label_set_text_static(dp_label, ui_cache.buf_dp);
        ui_dirty_flags &= ~UI_DIRTY_DP;
    }

    if (ui_dirty_flags & UI_DIRTY_EXP) {
        uint32_t exp_in_lvl = ui_cache.exp > ui_cache.exp_prev ? ui_cache.exp - ui_cache.exp_prev : 0;
        uint32_t exp_per_lvl = ui_cache.exp_next > ui_cache.exp_prev ? ui_cache.exp_next - ui_cache.exp_prev : 1;
        uint8_t xp_pct = (uint8_t)((exp_in_lvl * 100) / exp_per_lvl);
        if (xp_pct > 100) xp_pct = 100;
        lv_snprintf(ui_cache.buf_exp, sizeof(ui_cache.buf_exp), "XP %d%%", xp_pct);
        lv_label_set_text_static(exp_label, ui_cache.buf_exp);
        lv_bar_set_value(pet_xp_bar, xp_pct, LV_ANIM_ON);
        ui_dirty_flags &= ~UI_DIRTY_EXP;
    }

    if (ui_dirty_flags & UI_DIRTY_STATS) {
        lv_snprintf(ui_cache.buf_stats, sizeof(ui_cache.buf_stats), "STR: %d DEX: %d CON: %d INT: %d WIS: %d CHA: %d",
            ui_cache.stats[0], ui_cache.stats[1], ui_cache.stats[2],
            ui_cache.stats[3], ui_cache.stats[4], ui_cache.stats[5]);
        lv_label_set_text_static(stats_row, ui_cache.buf_stats);
        ui_dirty_flags &= ~UI_DIRTY_STATS;
    }

    if (ui_dirty_flags & UI_DIRTY_STATE) {
        if (ui_cache.state < 7) {
            strncpy(ui_cache.buf_combat_log, state_str[ui_cache.state], sizeof(ui_cache.buf_combat_log) - 1);
            ui_cache.buf_combat_log[sizeof(ui_cache.buf_combat_log) - 1] = '\0';
            lv_label_set_text_static(combat_log_label, ui_cache.buf_combat_log);
        }
        ui_dirty_flags &= ~UI_DIRTY_STATE;
    }

    if (ui_dirty_flags & UI_DIRTY_ENEMY_NAME) {
        if (ui_cache.enemy_visible) {
            strncpy(ui_cache.buf_enemy_name, ui_cache.enemy_name, sizeof(ui_cache.buf_enemy_name) - 1);
            ui_cache.buf_enemy_name[sizeof(ui_cache.buf_enemy_name) - 1] = '\0';
            lv_label_set_text_static(enemy_name_label, ui_cache.buf_enemy_name);
        } else {
            lv_label_set_text_static(enemy_name_label, "---");
        }
        ui_dirty_flags &= ~UI_DIRTY_ENEMY_NAME;
    }

    if (ui_dirty_flags & UI_DIRTY_ENEMY_HP) {
        if (ui_cache.enemy_visible) {
            lv_obj_clear_flag(enemy_sprite, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(enemy_shadow, LV_OBJ_FLAG_HIDDEN);
            int16_t hp_pct = ui_cache.enemy_hp_max > 0 ? (ui_cache.enemy_hp * 100) / ui_cache.enemy_hp_max : 0;
            lv_bar_set_value(enemy_hp_bar, hp_pct, LV_ANIM_ON);
            int anim_type = get_enemy_sprite_type(ui_cache.enemy_name);
            screens_set_enemy_animation(anim_type);
        } else {
            lv_obj_add_flag(enemy_sprite, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(enemy_shadow, LV_OBJ_FLAG_HIDDEN);
            lv_bar_set_value(enemy_hp_bar, 100, LV_ANIM_OFF);
        }
        ui_dirty_flags &= ~UI_DIRTY_ENEMY_HP;
    }
}

screen_id_e screens_get_current(void)
{
    return current_screen;
}

void screens_show_damage_popup(int16_t damage, bool is_critical)
{
    static char buf[16];
    if (is_critical) {
        lv_snprintf(buf, sizeof(buf), "%d!", damage);
    } else {
        lv_snprintf(buf, sizeof(buf), "%d", damage);
    }
    lv_label_set_text(enemy_damage_label, buf);
    lv_obj_clear_flag(enemy_damage_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_text_color(enemy_damage_label, lv_color_hex(COLOR_DAMAGE_RED), 0);
}

void screens_show_miss(void)
{
    lv_label_set_text(enemy_damage_label, "MISS");
    lv_obj_clear_flag(enemy_damage_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_text_color(enemy_damage_label, lv_color_hex(COLOR_WHITE), 0);
}

void screens_clear_damage_popup(void)
{
    lv_obj_add_flag(enemy_damage_label, LV_OBJ_FLAG_HIDDEN);
}

void screens_show_exp_popup(uint32_t exp)
{
    static char buf[16];
    lv_snprintf(buf, sizeof(buf), "+%lu XP", (unsigned long)exp);
    lv_label_set_text(pet_exp_label, buf);
    lv_obj_set_style_text_color(pet_exp_label, lv_color_hex(COLOR_EXP_POPUP_YELLOW), 0);
    lv_obj_clear_flag(pet_exp_label, LV_OBJ_FLAG_HIDDEN);
}

void screens_clear_exp_popup(void)
{
    lv_obj_add_flag(pet_exp_label, LV_OBJ_FLAG_HIDDEN);
}

void screens_set_pet_animation(int anim_type)
{
    switch (anim_type) {
    case 0:
        sprites_set_idle_animation(pet_sprite);
        if (rest_pet_sprite) sprites_set_idle_animation(rest_pet_sprite);
        break;
    case 1:
        sprites_set_attack_animation(pet_sprite);
        break;
    case 2:
        sprites_set_hit_animation(pet_sprite);
        break;
    case 3:
        sprites_set_death_animation(pet_sprite);
        if (rest_pet_sprite) sprites_set_death_animation(rest_pet_sprite);
        break;
    case 4:
        sprites_set_levelup_animation(pet_sprite);
        if (rest_pet_sprite) sprites_set_levelup_animation(rest_pet_sprite);
        break;
    default:
        sprites_set_idle_animation(pet_sprite);
        if (rest_pet_sprite) sprites_set_idle_animation(rest_pet_sprite);
        break;
    }
}

void screens_set_enemy_animation(int anim_type)
{
    switch (anim_type) {
    case 0:
        sprites_set_enemy_slime_animation(enemy_sprite);
        break;
    case 1:
        sprites_set_enemy_fish_animation(enemy_sprite);
        break;
    case 2:
        sprites_set_enemy_fly_animation(enemy_sprite);
        break;
    case 3:
        sprites_set_enemy_blocker_animation(enemy_sprite);
        break;
    case 4:
        sprites_set_enemy_poker_animation(enemy_sprite);
        break;
    default:
        sprites_set_enemy_slime_animation(enemy_sprite);
        break;
    }
}

void screens_show_rest_hp_popup(int16_t hp)
{
    if (hp <= 0) return;
    static char buf[16];
    lv_snprintf(buf, sizeof(buf), "+%d HP", hp);
    lv_label_set_text(rest_hp_popup, buf);
    lv_obj_clear_flag(rest_hp_popup, LV_OBJ_FLAG_HIDDEN);
}

void screens_show_rest_en_popup(int16_t en)
{
    if (en <= 0) return;
    static char buf[16];
    lv_snprintf(buf, sizeof(buf), "+%d EN", en);
    lv_label_set_text(rest_en_popup, buf);
    lv_obj_clear_flag(rest_en_popup, LV_OBJ_FLAG_HIDDEN);
}

void screens_clear_rest_popups(void)
{
    lv_obj_add_flag(rest_hp_popup, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(rest_en_popup, LV_OBJ_FLAG_HIDDEN);
}
