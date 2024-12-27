#pragma once

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

// EnviLog MQTT event group bits
#define ENVILOG_MQTT_CONNECTED_BIT     BIT0
#define ENVILOG_MQTT_DISCONNECTED_BIT  BIT1
#define ENVILOG_MQTT_ERROR_BIT         BIT2

/**
 * @brief Initialize the MQTT client
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t envilog_mqtt_init(void);

/**
 * @brief Start the MQTT client
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t envilog_mqtt_start(void);

/**
 * @brief Get MQTT connection status
 * 
 * @return true if connected
 */
bool envilog_mqtt_is_connected(void);

/**
 * @brief Get the MQTT event group handle
 * 
 * @return EventGroupHandle_t Event group handle
 */
EventGroupHandle_t envilog_mqtt_get_event_group(void);
