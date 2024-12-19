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
