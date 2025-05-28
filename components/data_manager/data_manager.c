#include "data_manager.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "data_manager";

// Internal state
static data_manager_config_t config = {0};
static dht11_reading_t latest_dht11_reading = {0};
static bool initialized = false;

esp_err_t data_manager_init(const data_manager_config_t *cfg) {
    if (cfg == NULL) {
        ESP_LOGE(TAG, "Configuration cannot be NULL");
        return ESP_ERR_INVALID_ARG;
    }

    // Store configuration
    config = *cfg;
    initialized = true;
    
    ESP_LOGI(TAG, "Data manager initialized successfully");
    return ESP_OK;
}

esp_err_t data_manager_publish_sensor_data(const char *source, const dht11_reading_t *reading) {
    if (!initialized) {
        ESP_LOGE(TAG, "Data manager not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (source == NULL || reading == NULL) {
        ESP_LOGE(TAG, "Invalid parameters");
        return ESP_ERR_INVALID_ARG;
    }

    // Handle DHT11 data
    if (strcmp(source, "dht11") == 0) {
        // Store latest reading
        latest_dht11_reading = *reading;
        
        ESP_LOGI(TAG, "Received DHT11 data: %.1f°C, %.1f%%RH", 
                 reading->temperature, reading->humidity);
        
        // Notify MQTT callback if configured
        if (config.mqtt_callback != NULL) {
            esp_err_t ret = config.mqtt_callback(reading);
            if (ret != ESP_OK) {
                ESP_LOGW(TAG, "MQTT callback failed: %s", esp_err_to_name(ret));
            }
        }
    }

    return ESP_OK;
}

esp_err_t data_manager_get_latest_data(const char *source, dht11_reading_t *reading) {
    if (!initialized) {
        ESP_LOGE(TAG, "Data manager not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (source == NULL || reading == NULL) {
        ESP_LOGE(TAG, "Invalid parameters");
        return ESP_ERR_INVALID_ARG;
    }

    // Handle DHT11 data request
    if (strcmp(source, "dht11") == 0) {
        *reading = latest_dht11_reading;
        return ESP_OK;
    }

    ESP_LOGW(TAG, "Unknown sensor source: %s", source);
    return ESP_ERR_NOT_FOUND;
}
