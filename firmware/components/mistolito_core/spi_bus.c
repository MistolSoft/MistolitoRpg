#include "spi_bus.h"
#include "esp_log.h"

static const char *TAG = "SPI_BUS";

static SemaphoreHandle_t spi_mutex = NULL;

void spi_bus_mutex_init(void)
{
    if (spi_mutex == NULL) {
        spi_mutex = xSemaphoreCreateRecursiveMutex();
        ESP_LOGI(TAG, "SPI bus mutex created");
    }
}

void spi_bus_lock(void)
{
    if (spi_mutex) {
        xSemaphoreTakeRecursive(spi_mutex, portMAX_DELAY);
    }
}

void spi_bus_unlock(void)
{
    if (spi_mutex) {
        xSemaphoreGiveRecursive(spi_mutex);
    }
}

SemaphoreHandle_t spi_bus_mutex_get(void)
{
    return spi_mutex;
}
