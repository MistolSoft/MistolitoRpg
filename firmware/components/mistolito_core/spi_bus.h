#ifndef SPI_BUS_H
#define SPI_BUS_H

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

void spi_bus_mutex_init(void);
void spi_bus_lock(void);
void spi_bus_unlock(void);
SemaphoreHandle_t spi_bus_mutex_get(void);

void spi_bus_suspend_lcd(void);
void spi_bus_resume_lcd(void);
bool spi_bus_is_heavy_activity(void);

#endif
