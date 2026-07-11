#include "training_mode.h"
#include "display_task.h"
#include "game_coordinator.h"
#include "screens.h"
#include "spi_bus.h"
#include "esp_log.h"
#include "lvgl.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "TRAIN";

static bool s_training_active = false;

static lv_obj_t *s_title_label = NULL;
static lv_obj_t *s_epoch_label = NULL;
static lv_obj_t *s_loss_label = NULL;
static lv_obj_t *s_transitions_label = NULL;
static lv_obj_t *s_time_label = NULL;
static lv_obj_t *s_status_label = NULL;

static void create_training_screen(void)
{
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_clean(scr);

    s_title_label = lv_label_create(scr);
    lv_label_set_text(s_title_label, "ENTRENAMIENTO");
    lv_obj_set_style_text_color(s_title_label, lv_color_make(0xFF, 0xD7, 0x00), 0);
    lv_obj_set_style_text_font(s_title_label, &lv_font_montserrat_14, 0);
    lv_obj_align(s_title_label, LV_ALIGN_TOP_MID, 0, 10);

    s_epoch_label = lv_label_create(scr);
    lv_label_set_text(s_epoch_label, "Epoch: 0/0");
    lv_obj_set_style_text_color(s_epoch_label, lv_color_white(), 0);
    lv_obj_align(s_epoch_label, LV_ALIGN_TOP_LEFT, 10, 40);

    s_loss_label = lv_label_create(scr);
    lv_label_set_text(s_loss_label, "Loss: 0.0000");
    lv_obj_set_style_text_color(s_loss_label, lv_color_white(), 0);
    lv_obj_align(s_loss_label, LV_ALIGN_TOP_LEFT, 10, 60);

    s_transitions_label = lv_label_create(scr);
    lv_label_set_text(s_transitions_label, "Trans: 0/0");
    lv_obj_set_style_text_color(s_transitions_label, lv_color_white(), 0);
    lv_obj_align(s_transitions_label, LV_ALIGN_TOP_LEFT, 10, 80);

    s_time_label = lv_label_create(scr);
    lv_label_set_text(s_time_label, "Time: 0:00");
    lv_obj_set_style_text_color(s_time_label, lv_color_white(), 0);
    lv_obj_align(s_time_label, LV_ALIGN_TOP_LEFT, 10, 100);

    s_status_label = lv_label_create(scr);
    lv_label_set_text(s_status_label, "Cargando datos...");
    lv_obj_set_style_text_color(s_status_label, lv_color_make(0x80, 0x80, 0x80), 0);
    lv_obj_align(s_status_label, LV_ALIGN_BOTTOM_MID, 0, -10);

    lv_timer_handler();
}

void training_mode_enter(void)
{
    if (s_training_active) {
        ESP_LOGW(TAG, "Already in training mode");
        return;
    }

    ESP_LOGI(TAG, "Entering training mode");

    if (g_coordinator_task_handle != NULL) {
        vTaskSuspend(g_coordinator_task_handle);
        ESP_LOGI(TAG, "Coordinator suspended");
    }

    if (g_display_task_handle != NULL) {
        vTaskSuspend(g_display_task_handle);
        ESP_LOGI(TAG, "Display task suspended");
    }

    spi_bus_lock();
    create_training_screen();
    spi_bus_unlock();

    s_training_active = true;
    ESP_LOGI(TAG, "Training mode active");
}

void training_mode_exit(bool accepted)
{
    if (!s_training_active) {
        ESP_LOGW(TAG, "Not in training mode");
        return;
    }

    ESP_LOGI(TAG, "Exiting training mode (accepted=%d)", accepted);

    s_training_active = false;

    spi_bus_lock();
    screens_load(SCREEN_GAME);
    spi_bus_unlock();

    if (g_display_task_handle != NULL) {
        vTaskResume(g_display_task_handle);
        ESP_LOGI(TAG, "Display task resumed");
    }

    if (g_coordinator_task_handle != NULL) {
        vTaskResume(g_coordinator_task_handle);
        ESP_LOGI(TAG, "Coordinator resumed");
    }

    ESP_LOGI(TAG, "Training mode exited");
}

bool training_mode_is_active(void)
{
    return s_training_active;
}

void training_mode_update_progress(const training_progress_t *progress)
{
    if (!s_training_active || progress == NULL) {
        return;
    }

    char buf[64];

    spi_bus_lock();

    snprintf(buf, sizeof(buf), "Epoch: %d/%d", progress->current_epoch, progress->total_epochs);
    lv_label_set_text(s_epoch_label, buf);

    snprintf(buf, sizeof(buf), "Loss: %.4f", progress->loss);
    lv_label_set_text(s_loss_label, buf);

    snprintf(buf, sizeof(buf), "Trans: %lu/%lu",
             (unsigned long)progress->transitions_processed,
             (unsigned long)progress->total_transitions);
    lv_label_set_text(s_transitions_label, buf);

    uint32_t seconds = progress->elapsed_ms / 1000;
    uint32_t minutes = seconds / 60;
    seconds %= 60;
    snprintf(buf, sizeof(buf), "Time: %lu:%02lu", (unsigned long)minutes, (unsigned long)seconds);
    lv_label_set_text(s_time_label, buf);

    lv_timer_handler();

    spi_bus_unlock();
}

void training_mode_update_status(const char *status)
{
    if (!s_training_active || status == NULL || s_status_label == NULL) {
        return;
    }

    spi_bus_lock();
    lv_label_set_text(s_status_label, status);
    lv_timer_handler();
    spi_bus_unlock();
}

void training_mode_lvgl_tick(void)
{
    if (!s_training_active) {
        return;
    }

    spi_bus_lock();
    lv_timer_handler();
    spi_bus_unlock();
}
