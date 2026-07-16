#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "mistolito.h"
#include "events.h"
#include "game_coordinator.h"
#include "storage_task.h"
#include "display_task.h"
#include "lcd_init.h"
#include "spi_bus.h"

static const char *TAG = "MAIN";

void app_main(void)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "MistolitoRPG v%s", MISTOLITO_VERSION);
    ESP_LOGI(TAG, "========================================");

    spi_bus_mutex_init();

    ESP_LOGI(TAG, "Initializing LCD hardware...");
    ESP_ERROR_CHECK(lcd_hardware_init());

    ESP_LOGI(TAG, "Initializing LVGL...");
    ESP_ERROR_CHECK(lvgl_display_init());

    ESP_LOGI(TAG, "Initializing storage...");
    storage_task_start();

    ESP_LOGI(TAG, "Starting coordinator...");
    game_coordinator_start();

    ESP_LOGI(TAG, "Creating tasks...");

    xTaskCreatePinnedToCore(game_coordinator_task, "coordinator", 12288, NULL, 4, &g_coordinator_task_handle, 0);

    xTaskCreatePinnedToCore(storage_task, "storage", 8192, NULL, 1, NULL, 0);

    xTaskCreatePinnedToCore(display_task, "display", 8192, NULL, 5, &g_display_task_handle, 1);

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "System running");
    ESP_LOGI(TAG, "========================================");
}
