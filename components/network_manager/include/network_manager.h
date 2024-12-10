#pragma once

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

// Network manager events
#define NETWORK_EVENT_WIFI_CONNECTED     BIT0
#define NETWORK_EVENT_WIFI_DISCONNECTED  BIT1
#define NETWORK_EVENT_SCAN_DONE         BIT2
#define NETWORK_EVENT_ERROR             BIT3

/**
 * @brief Initialize the network manager
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t network_manager_init(void);

/**
 * @brief Start the network manager task
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t network_manager_start(void);

/**
 * @brief Get the network event group handle
 * 
 * @return EventGroupHandle_t Event group handle
 */
EventGroupHandle_t network_manager_get_event_group(void);

/**
 * @brief Get current WiFi RSSI
 * 
 * @param rssi Pointer to store RSSI value
 * @return esp_err_t ESP_OK on success
 */
esp_err_t network_manager_get_rssi(int8_t *rssi);

/**
 * @brief Get current connection status
 * 
 * @return true if connected
 */
bool network_manager_is_connected(void);
