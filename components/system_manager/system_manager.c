#include "system_manager.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "envilog_config.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_chip_info.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_mac.h"
#include "driver/temperature_sensor.h"

static const char *TAG = "system_manager";
static nvs_handle_t nvs_config_handle;

// System start time for uptime calculation
static int64_t system_start_time;
static temperature_sensor_handle_t temp_sensor = NULL;

// NVS namespace and keys
#define NVS_NAMESPACE "envilog"
#define NVS_KEY_NETWORK_CONFIG "net_cfg"
#define NVS_KEY_MQTT_CONFIG "mqtt_cfg"
#define NVS_KEY_SYSTEM_CONFIG "sys_cfg"

// Default thresholds
#define DEFAULT_MIN_HEAP_THRESHOLD (10 * 1024)    // 10KB
#define DEFAULT_STACK_HWM_THRESHOLD (2 * 1024)    // 2KB

esp_err_t system_manager_init(void)
{
    esp_err_t ret;

    // Store system start time
    system_start_time = esp_timer_get_time();

    // Initialize temperature sensor with range -10°C to 80°C
    temperature_sensor_config_t temp_sensor_config = TEMPERATURE_SENSOR_CONFIG_DEFAULT(-10, 80);
    ret = temperature_sensor_install(&temp_sensor_config, &temp_sensor);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install temperature sensor: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = temperature_sensor_enable(temp_sensor);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable temperature sensor: %s", esp_err_to_name(ret));
        return ret;
    }

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

    // Enhanced system config with new thresholds
    system_config_t sys_cfg = {
        .task_wdt_timeout_ms = ENVILOG_TASK_WDT_TIMEOUT_MS,
        .diag_check_interval_ms = ENVILOG_DIAG_CHECK_INTERVAL_MS,
        .min_heap_threshold = DEFAULT_MIN_HEAP_THRESHOLD,
        .stack_hwm_threshold = DEFAULT_STACK_HWM_THRESHOLD
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

esp_err_t system_manager_get_diagnostics(system_diag_data_t *diag_data)
{
    if (diag_data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // Memory metrics
    diag_data->free_heap = esp_get_free_heap_size();
    diag_data->min_free_heap = esp_get_minimum_free_heap_size();

    // CPU metrics
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    diag_data->cpu_freq_mhz = CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ;
    
    // Calculate CPU usage (using task runtime stats)
    static uint32_t last_total_runtime = 0;
    uint32_t current_runtime = xTaskGetTickCount() * portTICK_PERIOD_MS;
    TaskStatus_t *task_stats = NULL;
    UBaseType_t task_count = uxTaskGetNumberOfTasks();
    uint32_t total_runtime = 0;
    
    task_stats = pvPortMalloc(task_count * sizeof(TaskStatus_t));
    if (task_stats != NULL) {
        task_count = uxTaskGetSystemState(task_stats, task_count, &total_runtime);
        if (last_total_runtime > 0) {
            uint32_t runtime_diff = total_runtime - last_total_runtime;
            diag_data->cpu_usage = runtime_diff ? ((float)runtime_diff / (current_runtime - last_total_runtime)) * 100.0f : 0.0f;
        } else {
            diag_data->cpu_usage = 0.0f;
        }
        last_total_runtime = total_runtime;
        vPortFree(task_stats);
    }

    // System metrics
    diag_data->uptime_seconds = (uint32_t)((esp_timer_get_time() - system_start_time) / 1000000);
    diag_data->task_count = task_count;

    // Temperature sensor reading
    float temp;
    if (temp_sensor != NULL && temperature_sensor_get_celsius(temp_sensor, &temp) == ESP_OK) {
        diag_data->internal_temp = temp;
    } else {
        diag_data->internal_temp = -999.0f; // Invalid temperature indicator
    }

    // Stack high water mark for the current task
    diag_data->stack_hwm = uxTaskGetStackHighWaterMark(NULL);

    return ESP_OK;
}

// Configuration load/save functions
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

esp_err_t system_manager_set_diag_interval(uint32_t interval_ms)
{
    if (interval_ms < 100 || interval_ms > 60000) { // 100ms to 60s range
        return ESP_ERR_INVALID_ARG;
    }

    system_config_t config;
    esp_err_t ret = system_manager_load_system_config(&config);
    if (ret != ESP_OK) {
        return ret;
    }

    config.diag_check_interval_ms = interval_ms;
    return system_manager_save_system_config(&config);
}
