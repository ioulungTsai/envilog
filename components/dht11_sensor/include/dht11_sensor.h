#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

// DHT11 data structure
typedef struct {
    float temperature;     // Temperature in Celsius
    float humidity;        // Humidity percentage
    uint64_t timestamp;    // Reading timestamp
    bool valid;           // Data validity flag
} dht11_reading_t;

/**
 * @brief Initialize DHT11 sensor
 * 
 * @param gpio_num GPIO pin number connected to DHT11
 * @return esp_err_t ESP_OK on success
 */
esp_err_t dht11_init(uint8_t gpio_num);

/**
 * @brief Read data from DHT11 sensor
 * 
 * @param reading Pointer to store sensor reading
 * @return esp_err_t ESP_OK on success
 */
esp_err_t dht11_read(dht11_reading_t *reading);

/**
 * @brief Start the DHT11 reading task
 * 
 * @param read_interval_ms Reading interval in milliseconds
 * @return esp_err_t ESP_OK on success
 */
esp_err_t dht11_start_reading(uint32_t read_interval_ms);

/**
 * @brief Stop the DHT11 reading task
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t dht11_stop_reading(void);

/**
 * @brief Get the last valid reading
 * 
 * @param reading Pointer to store the last reading
 * @return esp_err_t ESP_OK on success
 */
esp_err_t dht11_get_last_reading(dht11_reading_t *reading);
