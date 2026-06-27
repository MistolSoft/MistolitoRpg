#include "screens.h"
#include "sprites.h"
#include "mistolito.h"
#include "storage_task.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "SCREENS";

#define UI_ARENA_HEIGHT 160
#define UI_ARENA_Y 0
#define UI_STATS_HEIGHT 80
#define UI_STATS_Y 160
#define UI_PET_X 40
#define UI_PET_Y 50
#define UI_ENEMY_X 220
#define UI_ENEMY_Y 60

static lv_obj_t *screens[4] = {NULL, NULL, NULL, NULL};
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
static lv_obj_t *profession_label = NULL;
static lv_obj_t *hp_label = NULL;
static lv_obj_t *stats_header = NULL;
static lv_obj_t *stats_row = NULL;

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

#define REST_PET_X 124
#define REST_PET_Y 30
#define REST_STATS_Y 160

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
    uint8_t stats[STAT_COUNT];
    uint8_t profession;
    game_state_e state;
    char enemy_name[ENEMY_NAME_MAX_LEN];
    int16_t enemy_hp;
    int16_t enemy_hp_max;
    bool enemy_visible;
} ui_cache_t;

static ui_cache_t ui_cache = {0};
static uint16_t ui_dirty_flags = 0;

static void init_styles(void)
{
    lv_style_init(&style_box);
    lv_style_set_bg_color(&style_box, lv_color_hex(0x000000));
    lv_style_set_bg_opa(&style_box, LV_OPA_COVER);
    lv_style_set_border_width(&style_box, 1);
    lv_style_set_border_color(&style_box, lv_color_hex(0xFFFFFF));
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
    lv_obj_set_pos(lbl_init_title, 5, 5);
    lv_label_set_text(lbl_init_title, "MistolitoRPG v2.0");

    lbl_init_hint = lv_label_create(screens[SCREEN_INIT]);
    lv_obj_set_style_text_color(lbl_init_hint, lv_color_white(), 0);
    lv_obj_set_style_text_font(lbl_init_hint, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(lbl_init_hint, 5, 30);
    lv_label_set_text(lbl_init_hint, "Press BOOT to start");

    lbl_init_status = lv_label_create(screens[SCREEN_INIT]);
    lv_obj_set_style_text_color(lbl_init_status, lv_color_make(0xAA, 0xAA, 0xAA), 0);
    lv_obj_set_style_text_font(lbl_init_status, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(lbl_init_status, 5, 55);
    lv_label_set_text(lbl_init_status, "Waiting...");

    ESP_LOGI(TAG, "INIT screen created");
}

static void create_game_screen(void)
{
    screens[SCREEN_GAME] = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screens[SCREEN_GAME], lv_color_black(), 0);

    lv_obj_t *scr = screens[SCREEN_GAME];

    arena_bg = lv_obj_create(scr);
    lv_obj_set_size(arena_bg, LCD_WIDTH, UI_ARENA_HEIGHT);
    lv_obj_set_pos(arena_bg, 0, UI_ARENA_Y);
    lv_obj_set_style_bg_color(arena_bg, lv_color_hex(0xEBCE87), 0);
    lv_obj_set_style_border_width(arena_bg, 0, 0);
    lv_obj_set_style_radius(arena_bg, 0, 0);
    lv_obj_clear_flag(arena_bg, LV_OBJ_FLAG_SCROLLABLE);

    sprites_create_arena_background(arena_bg);

    pet_shadow = lv_obj_create(arena_bg);
    lv_obj_set_size(pet_shadow, 60, 10);
    lv_obj_set_pos(pet_shadow, UI_PET_X + 6, UI_PET_Y + 95);
    lv_obj_set_style_bg_color(pet_shadow, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(pet_shadow, LV_OPA_30, 0);
    lv_obj_set_style_border_width(pet_shadow, 0, 0);
    lv_obj_set_style_radius(pet_shadow, 5, 0);

    pet_sprite = lv_animimg_create(arena_bg);
    lv_obj_set_size(pet_sprite, 72, 97);
    lv_obj_set_pos(pet_sprite, UI_PET_X, UI_PET_Y);
    sprites_set_idle_animation(pet_sprite);

    enemy_shadow = lv_obj_create(arena_bg);
    lv_obj_set_size(enemy_shadow, 45, 8);
    lv_obj_set_pos(enemy_shadow, UI_ENEMY_X + 2, UI_ENEMY_Y + 26);
    lv_obj_set_style_bg_color(enemy_shadow, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(enemy_shadow, LV_OPA_30, 0);
    lv_obj_set_style_border_width(enemy_shadow, 0, 0);
    lv_obj_set_style_radius(enemy_shadow, 5, 0);
    lv_obj_add_flag(enemy_shadow, LV_OBJ_FLAG_HIDDEN);

    enemy_sprite = lv_animimg_create(arena_bg);
    lv_obj_set_size(enemy_sprite, 50, 28);
    lv_obj_set_pos(enemy_sprite, UI_ENEMY_X, UI_ENEMY_Y);
    lv_obj_add_flag(enemy_sprite, LV_OBJ_FLAG_HIDDEN);

    enemy_damage_label = lv_label_create(arena_bg);
    lv_label_set_text(enemy_damage_label, "");
    lv_obj_set_style_text_color(enemy_damage_label, lv_color_hex(0xF800), 0);
    lv_obj_set_style_text_font(enemy_damage_label, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(enemy_damage_label, UI_ENEMY_X + 10, UI_ENEMY_Y - 25);
    lv_obj_add_flag(enemy_damage_label, LV_OBJ_FLAG_HIDDEN);

    pet_exp_label = lv_label_create(arena_bg);
    lv_label_set_text(pet_exp_label, "");
    lv_obj_set_style_text_color(pet_exp_label, lv_color_hex(0xFFE0), 0);
    lv_obj_set_style_text_font(pet_exp_label, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(pet_exp_label, UI_PET_X, UI_PET_Y - 25);
    lv_obj_add_flag(pet_exp_label, LV_OBJ_FLAG_HIDDEN);

    enemy_name_label = lv_label_create(arena_bg);
    lv_label_set_text(enemy_name_label, "---");
    lv_obj_set_style_text_color(enemy_name_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(enemy_name_label, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(enemy_name_label, UI_ENEMY_X - 10, UI_ENEMY_Y + 35);

    enemy_hp_bar = lv_bar_create(arena_bg);
    lv_obj_set_size(enemy_hp_bar, 70, 8);
    lv_obj_set_pos(enemy_hp_bar, UI_ENEMY_X - 10, UI_ENEMY_Y + 53);
    lv_bar_set_range(enemy_hp_bar, 0, 100);
    lv_bar_set_value(enemy_hp_bar, 100, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(enemy_hp_bar, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_color(enemy_hp_bar, lv_color_hex(0xFFFFFF), LV_PART_INDICATOR);

    combat_log_label = lv_label_create(arena_bg);
    lv_label_set_text(combat_log_label, "");
    lv_obj_set_style_text_color(combat_log_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(combat_log_label, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(combat_log_label, 5, 5);

    stats_bg = lv_obj_create(scr);
    lv_obj_set_size(stats_bg, LCD_WIDTH, UI_STATS_HEIGHT);
    lv_obj_set_pos(stats_bg, 0, UI_STATS_Y);
    lv_obj_add_style(stats_bg, &style_box, 0);
    lv_obj_clear_flag(stats_bg, LV_OBJ_FLAG_SCROLLABLE);

    pet_name_label = lv_label_create(stats_bg);
    lv_label_set_text(pet_name_label, "Mistolito");
    lv_obj_set_style_text_color(pet_name_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(pet_name_label, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(pet_name_label, 5, 2);

    profession_label = lv_label_create(stats_bg);
    lv_label_set_text(profession_label, "NOV");
    lv_obj_set_style_text_color(profession_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(profession_label, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(profession_label, 90, 2);

    level_label = lv_label_create(stats_bg);
    lv_label_set_text(level_label, "Lv1");
    lv_obj_set_style_text_color(level_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(level_label, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(level_label, 125, 2);

    exp_label = lv_label_create(stats_bg);
    lv_label_set_text(exp_label, "XP0%");
    lv_obj_set_style_text_color(exp_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(exp_label, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(exp_label, 155, 2);

    dp_label = lv_label_create(stats_bg);
    lv_label_set_text(dp_label, "DP0");
    lv_obj_set_style_text_color(dp_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(dp_label, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(dp_label, 200, 2);

    hp_label = lv_label_create(stats_bg);
    lv_label_set_text(hp_label, "HP100%");
    lv_obj_set_style_text_color(hp_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(hp_label, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(hp_label, 5, 20);

    energy_label = lv_label_create(stats_bg);
    lv_label_set_text(energy_label, "EN100%");
    lv_obj_set_style_text_color(energy_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(energy_label, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(energy_label, 65, 20);

    stats_header = lv_label_create(stats_bg);
    lv_label_set_text(stats_header, "Str Dex Con Int Wis Cha");
    lv_obj_set_style_text_color(stats_header, lv_color_hex(0xAAAAAA), 0);
    lv_obj_set_style_text_font(stats_header, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(stats_header, 5, 40);

    stats_row = lv_label_create(stats_bg);
    lv_label_set_text(stats_row, " 10  10  10  10  10  10");
    lv_obj_set_style_text_color(stats_row, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(stats_row, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(stats_row, 5, 58);

    ESP_LOGI(TAG, "GAME screen created");
}

static void create_death_screen(void)
{
    screens[SCREEN_DEATH] = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screens[SCREEN_DEATH], lv_color_black(), 0);

    death_gameover_label = lv_label_create(screens[SCREEN_DEATH]);
    lv_obj_set_style_text_color(death_gameover_label, lv_color_hex(0xF800), 0);
    lv_obj_set_style_text_font(death_gameover_label, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(death_gameover_label, 70, 95);
    lv_label_set_text(death_gameover_label, "GAME OVER");

    death_resurrect_label = lv_label_create(screens[SCREEN_DEATH]);
    lv_obj_set_style_text_color(death_resurrect_label, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(death_resurrect_label, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(death_resurrect_label, 110, 135);
    lv_label_set_text(death_resurrect_label, "(resucitando...)");

    ESP_LOGI(TAG, "DEATH screen created");
}

static void create_rest_screen(void)
{
    screens[SCREEN_REST] = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screens[SCREEN_REST], lv_color_hex(0x1A1A2E), 0);

    lv_obj_t *scr = screens[SCREEN_REST];

    rest_bg = lv_obj_create(scr);
    lv_obj_set_size(rest_bg, LCD_WIDTH, REST_STATS_Y);
    lv_obj_set_pos(rest_bg, 0, 0);
    lv_obj_set_style_bg_color(rest_bg, lv_color_hex(0x1A1A2E), 0);
    lv_obj_set_style_border_width(rest_bg, 0, 0);
    lv_obj_set_style_radius(rest_bg, 0, 0);
    lv_obj_clear_flag(rest_bg, LV_OBJ_FLAG_SCROLLABLE);

    rest_pet_sprite = lv_animimg_create(rest_bg);
    lv_obj_set_size(rest_pet_sprite, 72, 97);
    lv_obj_set_pos(rest_pet_sprite, REST_PET_X, REST_PET_Y);
    sprites_set_idle_animation(rest_pet_sprite);

    rest_hp_popup = lv_label_create(rest_bg);
    lv_label_set_text(rest_hp_popup, "");
    lv_obj_set_style_text_color(rest_hp_popup, lv_color_hex(0x07E0), 0);
    lv_obj_set_style_text_font(rest_hp_popup, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(rest_hp_popup, REST_PET_X - 10, REST_PET_Y - 25);
    lv_obj_add_flag(rest_hp_popup, LV_OBJ_FLAG_HIDDEN);

    rest_en_popup = lv_label_create(rest_bg);
    lv_label_set_text(rest_en_popup, "");
    lv_obj_set_style_text_color(rest_en_popup, lv_color_hex(0x07FF), 0);
    lv_obj_set_style_text_font(rest_en_popup, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(rest_en_popup, REST_PET_X - 10, REST_PET_Y - 10);
    lv_obj_add_flag(rest_en_popup, LV_OBJ_FLAG_HIDDEN);

    rest_stats_bg = lv_obj_create(scr);
    lv_obj_set_size(rest_stats_bg, LCD_WIDTH, 80);
    lv_obj_set_pos(rest_stats_bg, 0, REST_STATS_Y);
    lv_obj_add_style(rest_stats_bg, &style_box, 0);
    lv_obj_clear_flag(rest_stats_bg, LV_OBJ_FLAG_SCROLLABLE);

    rest_name_label = lv_label_create(rest_stats_bg);
    lv_label_set_text(rest_name_label, "Mistolito");
    lv_obj_set_style_text_color(rest_name_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(rest_name_label, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(rest_name_label, 5, 2);

    rest_level_label = lv_label_create(rest_stats_bg);
    lv_label_set_text(rest_level_label, "Lv1");
    lv_obj_set_style_text_color(rest_level_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(rest_level_label, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(rest_level_label, 90, 2);

    rest_hp_label = lv_label_create(rest_stats_bg);
    lv_label_set_text(rest_hp_label, "HP100%");
    lv_obj_set_style_text_color(rest_hp_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(rest_hp_label, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(rest_hp_label, 5, 20);

    rest_en_label = lv_label_create(rest_stats_bg);
    lv_label_set_text(rest_en_label, "EN100%");
    lv_obj_set_style_text_color(rest_en_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(rest_en_label, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(rest_en_label, 65, 20);

    ESP_LOGI(TAG, "REST screen created");
}

void screens_init(void)
{
    init_styles();
    sprites_init();
    create_init_screen();
    create_game_screen();
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

void screens_update(game_snapshot_t *snap)
{
    static char buf[64];

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
            if (current_screen != SCREEN_GAME) {
                screens_load(SCREEN_GAME);
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
            lv_label_set_text(rest_name_label, ui_cache.name);
            ui_dirty_flags &= ~UI_DIRTY_NAME;
        }
        if (ui_dirty_flags & UI_DIRTY_LEVEL) {
            lv_snprintf(buf, sizeof(buf), "Lv%d", ui_cache.level);
            lv_label_set_text(rest_level_label, buf);
            ui_dirty_flags &= ~UI_DIRTY_LEVEL;
        }
        if (ui_dirty_flags & UI_DIRTY_HP) {
            uint8_t hp_pct = ui_cache.hp_max > 0 ? (ui_cache.hp * 100) / ui_cache.hp_max : 0;
            lv_snprintf(buf, sizeof(buf), "HP%d%%", hp_pct);
            lv_label_set_text(rest_hp_label, buf);
            ui_dirty_flags &= ~UI_DIRTY_HP;
        }
        if (ui_dirty_flags & UI_DIRTY_ENERGY) {
            uint8_t en_pct = ui_cache.energy_max > 0 ? (ui_cache.energy * 100) / ui_cache.energy_max : 0;
            lv_snprintf(buf, sizeof(buf), "EN%d%%", en_pct);
            lv_label_set_text(rest_en_label, buf);
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
    uint32_t trans_for_next = (uint32_t)snap->pet.level * 500;

    if (ui_cache.exp != total_trans || ui_cache.exp_next != trans_for_next) {
        ui_cache.exp = total_trans;
        ui_cache.exp_next = trans_for_next;
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

    if (ui_dirty_flags & UI_DIRTY_NAME) {
        lv_label_set_text(pet_name_label, ui_cache.name);
        ui_dirty_flags &= ~UI_DIRTY_NAME;
    }

    if (ui_dirty_flags & UI_DIRTY_LEVEL) {
        lv_snprintf(buf, sizeof(buf), "Lv%d", ui_cache.level);
        lv_label_set_text(level_label, buf);
        ui_dirty_flags &= ~UI_DIRTY_LEVEL;
    }

    if (ui_dirty_flags & UI_DIRTY_HP) {
        uint8_t hp_pct = ui_cache.hp_max > 0 ? (ui_cache.hp * 100) / ui_cache.hp_max : 0;
        lv_snprintf(buf, sizeof(buf), "HP%d%%", hp_pct);
        lv_label_set_text(hp_label, buf);
        ui_dirty_flags &= ~UI_DIRTY_HP;
    }

    if (ui_dirty_flags & UI_DIRTY_ENERGY) {
        uint8_t en_pct = ui_cache.energy_max > 0 ? (ui_cache.energy * 100) / ui_cache.energy_max : 0;
        lv_snprintf(buf, sizeof(buf), "EN%d%%", en_pct);
        lv_label_set_text(energy_label, buf);
        ui_dirty_flags &= ~UI_DIRTY_ENERGY;
    }

    if (ui_dirty_flags & UI_DIRTY_DP) {
        lv_snprintf(buf, sizeof(buf), "DP%lu", (unsigned long)ui_cache.dp);
        lv_label_set_text(dp_label, buf);
        ui_dirty_flags &= ~UI_DIRTY_DP;
    }

    if (ui_dirty_flags & UI_DIRTY_EXP) {
        uint8_t xp_pct = ui_cache.exp_next > 0 ? (ui_cache.exp * 100) / ui_cache.exp_next : 0;
        lv_snprintf(buf, sizeof(buf), "XP%d%%", xp_pct);
        lv_label_set_text(exp_label, buf);
        ui_dirty_flags &= ~UI_DIRTY_EXP;
    }

    if (ui_dirty_flags & UI_DIRTY_PROFESSION) {
        const char *prof_name = (ui_cache.profession < 4) ? prof_str[ui_cache.profession] : "???";
        lv_label_set_text(profession_label, prof_name);
        ui_dirty_flags &= ~UI_DIRTY_PROFESSION;
    }

    if (ui_dirty_flags & UI_DIRTY_STATS) {
        lv_snprintf(buf, sizeof(buf), " %2d %2d %2d %2d %2d %2d",
            ui_cache.stats[0], ui_cache.stats[1], ui_cache.stats[2],
            ui_cache.stats[3], ui_cache.stats[4], ui_cache.stats[5]);
        lv_label_set_text(stats_row, buf);
        ui_dirty_flags &= ~UI_DIRTY_STATS;
    }

    if (ui_dirty_flags & UI_DIRTY_STATE) {
        if (ui_cache.state < 7) {
            lv_label_set_text(combat_log_label, state_str[ui_cache.state]);
        }
        ui_dirty_flags &= ~UI_DIRTY_STATE;
    }

    if (ui_dirty_flags & UI_DIRTY_ENEMY_NAME) {
        if (ui_cache.enemy_visible) {
            lv_label_set_text(enemy_name_label, ui_cache.enemy_name);
        } else {
            lv_label_set_text(enemy_name_label, "---");
        }
        ui_dirty_flags &= ~UI_DIRTY_ENEMY_NAME;
    }

    if (ui_dirty_flags & UI_DIRTY_ENEMY_HP) {
        if (ui_cache.enemy_visible) {
            lv_obj_clear_flag(enemy_sprite, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(enemy_shadow, LV_OBJ_FLAG_HIDDEN);
            int16_t hp_pct = ui_cache.enemy_hp_max > 0 ? (ui_cache.enemy_hp * 100) / ui_cache.enemy_hp_max : 0;
            lv_bar_set_value(enemy_hp_bar, hp_pct, LV_ANIM_ON);
            sprites_set_enemy_slime_animation(enemy_sprite);
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
    lv_obj_set_style_text_color(enemy_damage_label, lv_color_hex(0xF800), 0);
}

void screens_show_miss(void)
{
    lv_label_set_text(enemy_damage_label, "MISS");
    lv_obj_clear_flag(enemy_damage_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_text_color(enemy_damage_label, lv_color_hex(0xFFFFFF), 0);
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
    lv_obj_set_style_text_color(pet_exp_label, lv_color_hex(0xFFE0), 0);
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
