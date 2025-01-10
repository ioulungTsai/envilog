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

static const char *TAG = "envilog";

// Function to print system diagnostics
static void print_diagnostics(void) {
    ESP_LOGI(TAG, "System Diagnostics:");
    ESP_LOGI(TAG, "- Free heap: %lu bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "- Minimum free heap: %lu bytes", esp_get_minimum_free_heap_size());
    ESP_LOGI(TAG, "- Running time: %lld ms", esp_timer_get_time() / 1000);
    ESP_LOGI(TAG, "- WiFi status: %s", network_manager_is_connected() ? "Connected" : "Disconnected");

    // Add WiFi RSSI when connected
    if (network_manager_is_connected()) {
        int8_t rssi;
        if (network_manager_get_rssi(&rssi) == ESP_OK) {
            ESP_LOGI(TAG, "- WiFi RSSI: %d dBm", rssi);
        }
    }

    // Add SPIFFS diagnostics
    size_t total = 0, used = 0;
    if (esp_spiffs_info(NULL, &total, &used) == ESP_OK) {
        ESP_LOGI(TAG, "- SPIFFS: %d KB used of %d KB", used/1024, total/1024);
    } else {
        ESP_LOGW(TAG, "- SPIFFS: Failed to get partition information");
    }

    // Add task manager diagnostics
    TaskStatus_t *task_status_array;
    UBaseType_t task_count = uxTaskGetNumberOfTasks();
    task_status_array = pvPortMalloc(task_count * sizeof(TaskStatus_t));
    
    if (task_status_array != NULL) {
        uint32_t total_runtime;
        task_count = uxTaskGetSystemState(task_status_array, task_count, &total_runtime);
        
        ESP_LOGI(TAG, "Task Status:");
        for (int i = 0; i < task_count; i++) {
            ESP_LOGI(TAG, "- %s: %s (Priority: %d)",
                     task_status_array[i].pcTaskName,
                     get_task_state_name(task_status_array[i].eCurrentState),
                     task_status_array[i].uxCurrentPriority);
        }
        
        vPortFree(task_status_array);
    }
}

// Timer callback for periodic diagnostics
static void diagnostic_timer_callback(void* arg) {
    print_diagnostics();
}

static void test_monitor_commands(void) {
    // Test getting heap info
    sys_monitor_cmd_msg_t cmd = {
        .cmd = SYS_MONITOR_CMD_GET_HEAP,
        .data = NULL,
        .data_len = 0
    };
    
    sys_monitor_resp_msg_t resp;
    
    ESP_LOGI(TAG, "Testing monitor commands...");
    
    if (system_monitor_send_command(&cmd, pdMS_TO_TICKS(1000)) == ESP_OK) {
        if (system_monitor_get_response(&resp, pdMS_TO_TICKS(1000)) == ESP_OK) {
            if (resp.status == ESP_OK && resp.data != NULL) {
                uint32_t *heap_info = (uint32_t*)resp.data;
                ESP_LOGI(TAG, "Heap info - Current: %lu, Minimum: %lu", 
                        heap_info[0], heap_info[1]);
                free(resp.data);  // Don't forget to free the allocated data
            }
        }
    }
    
    // Test changing interval
    uint32_t new_interval = 2000;  // 2 seconds
    cmd.cmd = SYS_MONITOR_CMD_SET_INTERVAL;
    cmd.data = &new_interval;
    cmd.data_len = sizeof(new_interval);
    
    if (system_monitor_send_command(&cmd, pdMS_TO_TICKS(1000)) == ESP_OK) {
        if (system_monitor_get_response(&resp, pdMS_TO_TICKS(1000)) == ESP_OK) {
            ESP_LOGI(TAG, "Interval change %s", 
                    resp.status == ESP_OK ? "successful" : "failed");
        }
    }
}

static esp_err_t init_spiffs(void)
{
    ESP_LOGI(TAG, "Initializing SPIFFS");

    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/www",
        .partition_label = NULL,  // Use default partition
        .max_files = 5,          // Maximum number of files that can be open at the same time
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

static void test_mqtt_queue(void)
{
    ESP_LOGI(TAG, "Starting MQTT queue test...");

    // Test status messages (QoS 0)
    for (int i = 0; i < 5; i++) {
        char status_msg[64];
        snprintf(status_msg, sizeof(status_msg), "Status update %d", i);
        ESP_LOGI(TAG, "Publishing status message %d", i);
        envilog_mqtt_publish_status(status_msg, 0);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    // Test diagnostic messages (QoS 1)
    for (int i = 0; i < 5; i++) {
        char diag_msg[64];
        snprintf(diag_msg, sizeof(diag_msg), "Diagnostic data %d", i);
        ESP_LOGI(TAG, "Publishing diagnostic message %d", i);
        envilog_mqtt_publish_diagnostic("test", diag_msg, 0);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    ESP_LOGI(TAG, "MQTT queue test completed");
}

// Network configuration test
static void test_network_config(void)
{
    ESP_LOGI(TAG, "Starting network configuration tests...");

    // Test A: First Boot - Already handled by network_manager logs
    ESP_LOGI(TAG, "Test A: First boot configuration");
    vTaskDelay(pdMS_TO_TICKS(5000));  // Wait for initial connection

    // Test B: Configuration Update
    ESP_LOGI(TAG, "Test B: Configuration update");
    network_config_t new_config = {
        .wifi_ssid = "TestSSID",
        .wifi_password = "TestPassword",
        .max_retry = 3,
        .conn_timeout_ms = 10000
    };
    
    esp_err_t ret = system_manager_save_network_config(&new_config);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "New configuration saved");
        ret = network_manager_update_config();
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "Network configuration updated");
        } else {
            ESP_LOGE(TAG, "Failed to update network configuration: %s", esp_err_to_name(ret));
        }
    } else {
        ESP_LOGE(TAG, "Failed to save new configuration: %s", esp_err_to_name(ret));
    }

    // Test C: Settings Persistence
    ESP_LOGI(TAG, "Test C: Please power cycle the device");
    ESP_LOGI(TAG, "After restart, check if settings persist");
}

// MQTT configuration test
static void test_mqtt_config(void)
{
   ESP_LOGI(TAG, "Starting MQTT configuration tests...");

   // Test D: First Boot - Check initial MQTT settings
   ESP_LOGI(TAG, "Test D: First boot MQTT configuration");
   vTaskDelay(pdMS_TO_TICKS(5000));  // Wait for initial connection

   // Test E: MQTT Configuration Update
   ESP_LOGI(TAG, "Test E: MQTT configuration update");
   mqtt_config_t new_config = {
       .broker_url = "mqtt://test.mosquitto.org",
       .client_id = "test_envilog",
       .keepalive = 60,
       .timeout_ms = 2000,
       .retry_timeout_ms = 1000
   };
   
   esp_err_t ret = system_manager_save_mqtt_config(&new_config);
   if (ret == ESP_OK) {
       ESP_LOGI(TAG, "New MQTT configuration saved");
       ret = envilog_mqtt_update_config();
       if (ret == ESP_OK) {
           ESP_LOGI(TAG, "MQTT configuration updated");
       } else {
           ESP_LOGE(TAG, "Failed to update MQTT configuration: %s", esp_err_to_name(ret));
       }
   } else {
       ESP_LOGE(TAG, "Failed to save new MQTT configuration: %s", esp_err_to_name(ret));
   }

   // Test F: MQTT Settings Persistence
   ESP_LOGI(TAG, "Test F: Please power cycle the device");
   ESP_LOGI(TAG, "After restart, check if MQTT settings persist");
}

static void test_config_operations(void)
{
    ESP_LOGI(TAG, "Testing configuration operations...");
    
    // Test network configuration
    network_config_t net_cfg;
    esp_err_t ret = system_manager_load_network_config(&net_cfg);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Current WiFi SSID: %s", net_cfg.wifi_ssid);
        ESP_LOGI(TAG, "Max retry: %u", net_cfg.max_retry);
    }

    // Test MQTT configuration
    mqtt_config_t mqtt_cfg;
    ret = system_manager_load_mqtt_config(&mqtt_cfg);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "MQTT Broker URL: %s", mqtt_cfg.broker_url);
        ESP_LOGI(TAG, "MQTT Client ID: %s", mqtt_cfg.client_id);
    }

    // Test system configuration
    system_config_t sys_cfg;
    ret = system_manager_load_system_config(&sys_cfg);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Task WDT timeout: %lu ms", sys_cfg.task_wdt_timeout_ms);
        ESP_LOGI(TAG, "Diagnostic interval: %lu ms", sys_cfg.diag_check_interval_ms);
    }
}
        
void app_main(void)
{
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

    // Run system manager test
    test_config_operations();

    // Initialize SPIFFS - Add here
    ESP_ERROR_CHECK(init_spiffs());
    ESP_LOGI(TAG, "SPIFFS initialized successfully");

    // Initialize TWDT here
    esp_task_wdt_config_t twdt_config = {
        .timeout_ms = ENVILOG_TASK_WDT_TIMEOUT_MS,
        .idle_core_mask = 0,     // Don't watch idle tasks
        .trigger_panic = true    // Trigger panic on timeout
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

    // Run Network configuration test
    // test_network_config();

    // Initialize and start MQTT client
    ESP_ERROR_CHECK(envilog_mqtt_init());
    ESP_ERROR_CHECK(envilog_mqtt_start());
    ESP_LOGI(TAG, "MQTT client started");
    
    // Run MQTT configuration test
    // test_mqtt_config();

    // Wait a bit for MQTT to connect
    vTaskDelay(pdMS_TO_TICKS(2000));

    // Run MQTT queue test
    test_mqtt_queue();

    // Initialize HTTP server
    http_server_config_t http_config = {
        .port = 80,
        .max_clients = 4,
        .enable_cors = true
    };

    ESP_LOGI(TAG, "Starting HTTP server...");
    esp_err_t http_ret = http_server_init(&http_config);  // Changed from ret to http_ret
    if (http_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(http_ret));
        return;
    }

    // Create periodic timer for diagnostics
    const esp_timer_create_args_t timer_args = {
        .callback = diagnostic_timer_callback,
        .name = "diagnostic_timer"
    };
    esp_timer_handle_t diagnostic_timer;
    
    ret = esp_timer_create(&timer_args, &diagnostic_timer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create diagnostic timer: %s", esp_err_to_name(ret));
        return;
    }

    // Start periodic diagnostics
    ret = esp_timer_start_periodic(diagnostic_timer, ENVILOG_DIAG_CHECK_INTERVAL_MS * 1000);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start diagnostic timer: %s", esp_err_to_name(ret));
        return;
    }

    ESP_LOGI(TAG, "System initialized successfully");
    print_diagnostics();

    test_monitor_commands();

    // Main loop - can be used for future main task operations
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
