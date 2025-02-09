#pragma once

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

// EnviLog MQTT event group bits
#define ENVILOG_MQTT_CONNECTED_BIT     BIT0
#define ENVILOG_MQTT_DISCONNECTED_BIT  BIT1
#define ENVILOG_MQTT_ERROR_BIT         BIT2

// MQTT Topic definitions
#define ENVILOG_MQTT_TOPIC_ROOT         "/envilog"
#define ENVILOG_MQTT_TOPIC_STATUS       "/envilog/status"
#define ENVILOG_MQTT_TOPIC_DIAGNOSTIC   "/envilog/diagnostic"
#define ENVILOG_MQTT_TOPIC_MAX_LEN      64
#define ENVILOG_MQTT_TOPIC_SENSORS      "/envilog/sensors"
#define ENVILOG_MQTT_TOPIC_DHT11        "/envilog/sensors/dht11"
#define ENVILOG_MQTT_TOPIC_SENSOR_CONFIG "/envilog/sensors/config"

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

/**
 * @brief Publish status message (QoS 0)
 * 
 * @param data Status message
 * @param len Data length (0 for null-terminated string)
 * @return esp_err_t ESP_OK on success
 */
esp_err_t envilog_mqtt_publish_status(const char *data, size_t len);

/**
 * @brief Publish diagnostic data (QoS 1 for testing queue)
 * 
 * @param type Diagnostic type (e.g., "heap", "wifi", "system")
 * @param data Diagnostic data
 * @param len Data length (0 for null-terminated string)
 * @return esp_err_t ESP_OK on success
 */
esp_err_t envilog_mqtt_publish_diagnostic(const char *type, const char *data, size_t len);

/**
 * @brief Update MQTT configuration
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t envilog_mqtt_update_config(void);
