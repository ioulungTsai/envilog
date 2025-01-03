#include "system_manager.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "envilog_config.h"

static const char *TAG = "system_manager";
static nvs_handle_t nvs_config_handle;

// NVS namespace and keys
#define NVS_NAMESPACE "envilog"
#define NVS_KEY_NETWORK_CONFIG "net_cfg"
#define NVS_KEY_MQTT_CONFIG "mqtt_cfg"
#define NVS_KEY_SYSTEM_CONFIG "sys_cfg"

esp_err_t system_manager_init(void)
{
    esp_err_t ret;

    // Open NVS handle
    ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_config_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error opening NVS handle: %s", esp_err_to_name(ret));
        return ret;
    }

    // Initialize default configurations if not exists
    network_config_t net_cfg = {
        .wifi_ssid = ENVILOG_WIFI_SSID,
        .wifi_password = ENVILOG_WIFI_PASS,
        .max_retry = ENVILOG_WIFI_RETRY_NUM,
        .conn_timeout_ms = ENVILOG_WIFI_CONN_TIMEOUT_MS
    };

    mqtt_config_t mqtt_cfg = {
        .broker_url = ENVILOG_MQTT_BROKER_URL,
        .client_id = ENVILOG_MQTT_CLIENT_ID,
        .keepalive = ENVILOG_MQTT_KEEPALIVE,
        .timeout_ms = ENVILOG_MQTT_TIMEOUT_MS,
        .retry_timeout_ms = ENVILOG_MQTT_RETRY_TIMEOUT_MS
    };

    system_config_t sys_cfg = {
        .task_wdt_timeout_ms = ENVILOG_TASK_WDT_TIMEOUT_MS,
        .diag_check_interval_ms = ENVILOG_DIAG_CHECK_INTERVAL_MS
    };

    // Try to load existing configs, save defaults if not found
    if (system_manager_load_network_config(NULL) != ESP_OK) {
        ret = system_manager_save_network_config(&net_cfg);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to save default network config");
        }
    }

    if (system_manager_load_mqtt_config(NULL) != ESP_OK) {
        ret = system_manager_save_mqtt_config(&mqtt_cfg);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to save default MQTT config");
        }
    }

    if (system_manager_load_system_config(NULL) != ESP_OK) {
        ret = system_manager_save_system_config(&sys_cfg);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to save default system config");
        }
    }

    ESP_LOGI(TAG, "System manager initialized");
    return ESP_OK;
}

// Implementation of config load/save functions
esp_err_t system_manager_load_network_config(network_config_t *config)
{
    size_t required_size = 0;
    esp_err_t ret = nvs_get_blob(nvs_config_handle, NVS_KEY_NETWORK_CONFIG, NULL, &required_size);
    
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        return ESP_ERR_NOT_FOUND;
    }

    if (config == NULL) {
        return ret;
    }

    if (required_size != sizeof(network_config_t)) {
        return ESP_ERR_INVALID_SIZE;
    }

    return nvs_get_blob(nvs_config_handle, NVS_KEY_NETWORK_CONFIG, config, &required_size);
}

esp_err_t system_manager_save_network_config(const network_config_t *config)
{
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = nvs_set_blob(nvs_config_handle, NVS_KEY_NETWORK_CONFIG, config, sizeof(network_config_t));
    if (ret != ESP_OK) {
        return ret;
    }

    return nvs_commit(nvs_config_handle);
}

esp_err_t system_manager_load_mqtt_config(mqtt_config_t *config)
{
    size_t required_size = 0;
    esp_err_t ret = nvs_get_blob(nvs_config_handle, NVS_KEY_MQTT_CONFIG, NULL, &required_size);
    
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        return ESP_ERR_NOT_FOUND;
    }

    if (config == NULL) {
        return ret;
    }

    if (required_size != sizeof(mqtt_config_t)) {
        return ESP_ERR_INVALID_SIZE;
    }

    return nvs_get_blob(nvs_config_handle, NVS_KEY_MQTT_CONFIG, config, &required_size);
}

esp_err_t system_manager_save_mqtt_config(const mqtt_config_t *config)
{
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = nvs_set_blob(nvs_config_handle, NVS_KEY_MQTT_CONFIG, config, sizeof(mqtt_config_t));
    if (ret != ESP_OK) {
        return ret;
    }

    return nvs_commit(nvs_config_handle);
}

esp_err_t system_manager_load_system_config(system_config_t *config)
{
    size_t required_size = 0;
    esp_err_t ret = nvs_get_blob(nvs_config_handle, NVS_KEY_SYSTEM_CONFIG, NULL, &required_size);
    
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        return ESP_ERR_NOT_FOUND;
    }

    if (config == NULL) {
        return ret;
    }

    if (required_size != sizeof(system_config_t)) {
        return ESP_ERR_INVALID_SIZE;
    }

    return nvs_get_blob(nvs_config_handle, NVS_KEY_SYSTEM_CONFIG, config, &required_size);
}

esp_err_t system_manager_save_system_config(const system_config_t *config)
{
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = nvs_set_blob(nvs_config_handle, NVS_KEY_SYSTEM_CONFIG, config, sizeof(system_config_t));
    if (ret != ESP_OK) {
        return ret;
    }

    return nvs_commit(nvs_config_handle);
}
