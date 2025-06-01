#pragma once

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

// Network manager events
#define NETWORK_EVENT_WIFI_CONNECTED     BIT0
#define NETWORK_EVENT_WIFI_DISCONNECTED  BIT1
#define NETWORK_EVENT_SCAN_DONE         BIT2
#define NETWORK_EVENT_ERROR             BIT3
#define NETWORK_EVENT_AP_STARTED        BIT4  // New: AP mode started
#define NETWORK_EVENT_AP_STOPPED        BIT5  // New: AP mode stopped

// Network operation modes
typedef enum {
    NETWORK_MODE_STATION = 0,      // Station mode (connects to WiFi)
    NETWORK_MODE_AP,               // Access Point mode (creates hotspot)
    NETWORK_MODE_SWITCHING         // Transitioning between modes
} network_mode_t;

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
 * @brief Get current WiFi RSSI (Station mode only)
 * 
 * @param rssi Pointer to store RSSI value
 * @return esp_err_t ESP_OK on success
 */
esp_err_t network_manager_get_rssi(int8_t *rssi);

/**
 * @brief Get current connection status (Station mode)
 * 
 * @return true if connected in station mode
 */
bool network_manager_is_connected(void);

/**
 * @brief Update network configuration
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t network_manager_update_config(void);

// New dual-mode functions

/**
 * @brief Get current network operation mode
 * 
 * @return network_mode_t Current mode
 */
network_mode_t network_manager_get_mode(void);

/**
 * @brief Check if WiFi credentials are configured in NVS
 * 
 * @return true if valid credentials are stored
 */
bool network_manager_is_provisioned(void);

/**
 * @brief Start AP mode for configuration
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t network_manager_start_ap_mode(void);

/**
 * @brief Switch to Station mode (will use stored credentials)
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t network_manager_switch_to_station(void);
