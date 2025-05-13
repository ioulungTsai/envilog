#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_task_wdt.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "envilog_config.h"
#include "task_manager.h"
#include "network_manager.h"
#include "system_monitor_msg.h"
#include "http_server.h"
#include "esp_spiffs.h"
#include "envilog_mqtt.h"
#include "system_manager.h"
#include "dht11_sensor.h"
#include "cJSON.h"

static const char *TAG = "envilog";

// Function declarations
static esp_err_t init_spiffs(void);
static void publish_sensor_diagnostics(void);

static esp_err_t init_spiffs(void) {
    ESP_LOGI(TAG, "Initializing SPIFFS");

    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/www",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = true
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return ret;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(NULL, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    return ESP_OK;
}

static void publish_sensor_diagnostics(void) {
    dht11_reading_t reading;
    if (dht11_get_last_reading(&reading) == ESP_OK && reading.valid) {
        // Create JSON string for sensor data
        cJSON *root = cJSON_CreateObject();
        if (root) {
            cJSON_AddNumberToObject(root, "temperature", reading.temperature);
            cJSON_AddNumberToObject(root, "humidity", reading.humidity);
            cJSON_AddNumberToObject(root, "timestamp", reading.timestamp);
            
            char *json_str = cJSON_PrintUnformatted(root);
            if (json_str) {
                // Publish to MQTT if connected
                if (envilog_mqtt_is_connected()) {
                    envilog_mqtt_publish_diagnostic("sensors/dht11", json_str, strlen(json_str));
                }
                free(json_str);
            }
            cJSON_Delete(root);
        }
    }
}

void app_main(void) {
    // Initialize logging
    esp_log_level_set(TAG, ESP_LOG_INFO);
    ESP_LOGI(TAG, "EnviLog v%d.%d.%d starting...", 
             ENVILOG_VERSION_MAJOR, ENVILOG_VERSION_MINOR, ENVILOG_VERSION_PATCH);

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize system manager
    ESP_ERROR_CHECK(system_manager_init());
    ESP_LOGI(TAG, "System manager initialized");

    // Initialize SPIFFS
    ESP_ERROR_CHECK(init_spiffs());
    ESP_LOGI(TAG, "SPIFFS initialized successfully");

    // Initialize TWDT
    esp_task_wdt_config_t twdt_config = {
        .timeout_ms = ENVILOG_TASK_WDT_TIMEOUT_MS,
        .idle_core_mask = 0,
        .trigger_panic = true
    };
    ESP_ERROR_CHECK(esp_task_wdt_reconfigure(&twdt_config));
    ESP_LOGI(TAG, "Task watchdog reconfigured");

    // Initialize event loop
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Initialize task manager
    ESP_ERROR_CHECK(task_manager_init());
    ESP_LOGI(TAG, "Task manager initialized");

    // Initialize system monitor queues
    ESP_ERROR_CHECK(system_monitor_queue_init());
    ESP_LOGI(TAG, "System monitor queues initialized");

    // Create system monitor task
    ESP_ERROR_CHECK(create_system_monitor_task());
    ESP_LOGI(TAG, "System monitor task created");

    // Initialize and start network manager
    ESP_ERROR_CHECK(network_manager_init());
    ESP_ERROR_CHECK(network_manager_start());
    ESP_LOGI(TAG, "Network manager started");

    // Initialize and start MQTT client
    ESP_ERROR_CHECK(envilog_mqtt_init());
    ESP_ERROR_CHECK(envilog_mqtt_start());
    ESP_LOGI(TAG, "MQTT client started");

    // Initialize DHT11 sensor
    ESP_LOGI(TAG, "Initializing DHT11 sensor...");
    ret = dht11_init(CONFIG_DHT11_GPIO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize DHT11: %s", esp_err_to_name(ret));
    } else {
        ret = dht11_start_reading(CONFIG_DHT11_READ_INTERVAL);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to start DHT11 readings: %s", esp_err_to_name(ret));
        } else {
            ESP_LOGI(TAG, "DHT11 sensor started successfully");
        }
    }

    // Initialize HTTP server
    http_server_config_t http_config = {
        .port = 80,
        .max_clients = 4,
        .enable_cors = true
    };

    ESP_LOGI(TAG, "Starting HTTP server...");
    ret = http_server_init(&http_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(ret));
        return;
    }

    ESP_LOGI(TAG, "Starting diagnostics system...");
    ret = system_manager_start_diagnostics(ENVILOG_DIAG_CHECK_INTERVAL_MS);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start diagnostics: %s", esp_err_to_name(ret));
        return;
    }

    ESP_LOGI(TAG, "System initialized successfully");
    system_manager_print_diagnostics();

    // Main loop - can be used for future main task operations
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
