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
#include "envilog_config.h"
#include "task_manager.h"
#include "network_manager.h"
#include "system_monitor_msg.h"
#include "http_server.h"
#include "envilog_mqtt.h"
#include "system_manager.h"
#include "dht11_sensor.h"
#include "error_handler.h"
#include "data_manager.h"

static const char *TAG = "envilog";

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

    // Initialize Data Manager with MQTT callback
    ESP_LOGI(TAG, "Initializing Data Manager...");
    data_manager_config_t data_config = {
        .mqtt_callback = envilog_mqtt_get_sensor_callback(),
        .http_getter = NULL  // HTTP uses direct API calls
    };

    ret = data_manager_init(&data_config);
    
    if (ret != ESP_OK) {
        ERROR_LOG_ERROR(TAG, ret, ERROR_CAT_SYSTEM, "Failed to initialize Data Manager");
        return;
    }

    // Initialize DHT11 sensor
    ESP_LOGI(TAG, "Initializing DHT11 sensor...");
    ret = dht11_init(CONFIG_DHT11_GPIO);
    if (ret != ESP_OK) {
        ERROR_LOG_ERROR(TAG, ret, ERROR_CAT_SENSOR, "Failed to initialize DHT11");
    } else {
        ret = dht11_start_reading(CONFIG_DHT11_READ_INTERVAL);
        if (ret != ESP_OK) {
            ERROR_LOG_ERROR(TAG, ret, ERROR_CAT_SENSOR, "Failed to start DHT11 readings");
        } else {
            ESP_LOGI(TAG, "DHT11 sensor started successfully");
        }
    }

    ESP_LOGI(TAG, "Starting HTTP server...");
    ret = http_server_init_default();
    if (ret != ESP_OK) {
        ERROR_LOG_ERROR(TAG, ret, ERROR_CAT_COMMUNICATION, "Failed to start HTTP server");
        return;
    }

    ESP_LOGI(TAG, "Starting diagnostics system...");
    ret = system_manager_start_diagnostics(ENVILOG_DIAG_CHECK_INTERVAL_MS);
    if (ret != ESP_OK) {
        ERROR_LOG_ERROR(TAG, ret, ERROR_CAT_SYSTEM, "Failed to start diagnostics");
        return;
    }

    ESP_LOGI(TAG, "System initialized successfully");
    system_manager_print_diagnostics();

    // Main loop - can be used for future main task operations
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
