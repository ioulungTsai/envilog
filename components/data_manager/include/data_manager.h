#pragma once

#include "esp_err.h"
#include "dht11_sensor.h"
#include <stdint.h>
#include <stdbool.h>

// Data consumer callback types
typedef esp_err_t (*sensor_data_callback_t)(const dht11_reading_t *reading);
typedef esp_err_t (*sensor_data_getter_t)(dht11_reading_t *reading);

/**
 * @brief Data manager configuration
 */
typedef struct {
    sensor_data_callback_t mqtt_callback;    // Called when new sensor data arrives
    sensor_data_getter_t http_getter;        // Called by HTTP to get latest data
} data_manager_config_t;

/**
 * @brief Initialize the data manager
 * 
 * @param config Configuration with callbacks
 * @return esp_err_t ESP_OK on success
 */
esp_err_t data_manager_init(const data_manager_config_t *config);

/**
 * @brief Process new sensor data (called by sensors)
 * 
 * @param source Sensor source name (e.g., "dht11")
 * @param reading Sensor reading data
 * @return esp_err_t ESP_OK on success
 */
esp_err_t data_manager_publish_sensor_data(const char *source, const dht11_reading_t *reading);

/**
 * @brief Get latest sensor data (called by HTTP server)
 * 
 * @param source Sensor source name (e.g., "dht11")
 * @param reading Pointer to store latest reading
 * @return esp_err_t ESP_OK on success
 */
esp_err_t data_manager_get_latest_data(const char *source, dht11_reading_t *reading);
