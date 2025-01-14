#pragma once

#include <esp_err.h>
#include <stdint.h>
#include <stdbool.h>

// Configuration structure for network settings
typedef struct {
    char wifi_ssid[32];
    char wifi_password[64];
    uint8_t max_retry;
    uint32_t conn_timeout_ms;
} network_config_t;

// Configuration structure for MQTT settings
typedef struct {
    char broker_url[128];
    char client_id[32];
    uint16_t keepalive;
    uint32_t timeout_ms;
    uint32_t retry_timeout_ms;
} mqtt_config_t;

// Configuration structure for system settings
typedef struct {
    uint32_t task_wdt_timeout_ms;
    uint32_t diag_check_interval_ms;
    uint32_t min_heap_threshold;        // New: Minimum heap threshold for warnings
    uint32_t stack_hwm_threshold;       // New: Stack high water mark threshold
} system_config_t;

// Configuration structure for System diagnostics data 
typedef struct {
    uint32_t free_heap;
    uint32_t min_free_heap;
    uint32_t task_count;
    uint32_t cpu_freq_mhz;
    float cpu_usage;
    uint32_t uptime_seconds;
    uint32_t stack_hwm;                 // Stack high water mark
    float internal_temp;
} system_diag_data_t;

/**
 * @brief Initialize the system manager
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t system_manager_init(void);

/**
 * @brief Load network configuration from NVS
 * 
 * @param config Pointer to network_config_t structure
 * @return esp_err_t ESP_OK on success
 */
esp_err_t system_manager_load_network_config(network_config_t *config);

/**
 * @brief Save network configuration to NVS
 * 
 * @param config Pointer to network_config_t structure
 * @return esp_err_t ESP_OK on success
 */
esp_err_t system_manager_save_network_config(const network_config_t *config);

/**
 * @brief Load MQTT configuration from NVS
 * 
 * @param config Pointer to mqtt_config_t structure
 * @return esp_err_t ESP_OK on success
 */
esp_err_t system_manager_load_mqtt_config(mqtt_config_t *config);

/**
 * @brief Save MQTT configuration to NVS
 * 
 * @param config Pointer to mqtt_config_t structure
 * @return esp_err_t ESP_OK on success
 */
esp_err_t system_manager_save_mqtt_config(const mqtt_config_t *config);

/**
 * @brief Load system configuration from NVS
 * 
 * @param config Pointer to system_config_t structure
 * @return esp_err_t ESP_OK on success
 */
esp_err_t system_manager_load_system_config(system_config_t *config);

/**
 * @brief Save system configuration to NVS
 * 
 * @param config Pointer to system_config_t structure
 * @return esp_err_t ESP_OK on success
 */
esp_err_t system_manager_save_system_config(const system_config_t *config);

/**
 * @brief Get current system diagnostics data
 * 
 * @param diag_data Pointer to system_diag_data_t structure
 * @return esp_err_t ESP_OK on success
 */
esp_err_t system_manager_get_diagnostics(system_diag_data_t *diag_data);

/**
 * @brief Update system diagnostics check interval
 * 
 * @param interval_ms New interval in milliseconds
 * @return esp_err_t ESP_OK on success
 */
esp_err_t system_manager_set_diag_interval(uint32_t interval_ms);
