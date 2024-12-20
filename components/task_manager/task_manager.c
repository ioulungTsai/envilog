#include <string.h>
#include "esp_log.h"
#include "esp_system.h"
#include "task_manager.h"
#include "esp_timer.h"
#include "esp_task_wdt.h"
#include "system_monitor_msg.h"

static const char *TAG = "task_manager";

// System event group
static EventGroupHandle_t system_event_group = NULL;

// Task handles
static TaskHandle_t system_monitor_handle = NULL;
// We'll uncomment this when implementing the network task
// static TaskHandle_t network_task_handle = NULL;

// System monitor task implementation
static void system_monitor_task(void *pvParameters) {
    TickType_t last_wake_time = xTaskGetTickCount();
    TickType_t check_interval = pdMS_TO_TICKS(1000); // 1 second interval
    sys_monitor_cmd_msg_t cmd_msg;
    sys_monitor_resp_msg_t resp_msg;

    ESP_LOGI(TAG, "System monitor task started");

    // Subscribe this task to watchdog
    ESP_ERROR_CHECK(esp_task_wdt_add(NULL));  // NULL means current task

    while (1) {
        // Reset watchdog at start of loop
        ESP_ERROR_CHECK(esp_task_wdt_reset());

        // Check for commands (non-blocking)
        if (xQueueReceive(system_monitor_cmd_queue, &cmd_msg, 0) == pdTRUE) {
            switch (cmd_msg.cmd) {
                case SYS_MONITOR_CMD_GET_HEAP: {
                    // Prepare heap status response
                    resp_msg.type = SYS_MONITOR_RESP_HEAP;
                    resp_msg.status = ESP_OK;
                    uint32_t *heap_info = malloc(2 * sizeof(uint32_t));
                    if (heap_info != NULL) {
                        heap_info[0] = esp_get_free_heap_size();
                        heap_info[1] = esp_get_minimum_free_heap_size();
                        resp_msg.data = heap_info;
                        resp_msg.data_len = 2 * sizeof(uint32_t);
                    } else {
                        resp_msg.status = ESP_ERR_NO_MEM;
                        resp_msg.data = NULL;
                        resp_msg.data_len = 0;
                    }
                    xQueueSend(system_monitor_resp_queue, &resp_msg, portMAX_DELAY);
                    break;
                }
                
                case SYS_MONITOR_CMD_SET_INTERVAL: {
                    // Update monitoring interval
                    if (cmd_msg.data && cmd_msg.data_len == sizeof(uint32_t)) {
                        uint32_t new_interval = *(uint32_t*)cmd_msg.data;
                        if (new_interval >= 100 && new_interval <= 10000) {  // 100ms to 10s
                            check_interval = pdMS_TO_TICKS(new_interval);
                            resp_msg.status = ESP_OK;
                        } else {
                            resp_msg.status = ESP_ERR_INVALID_ARG;
                        }
                    } else {
                        resp_msg.status = ESP_ERR_INVALID_ARG;
                    }
                    resp_msg.type = SYS_MONITOR_RESP_DIAG;
                    resp_msg.data = NULL;
                    resp_msg.data_len = 0;
                    xQueueSend(system_monitor_resp_queue, &resp_msg, portMAX_DELAY);
                    break;
                }

                case SYS_MONITOR_CMD_GET_TASKS:
                case SYS_MONITOR_CMD_GET_WIFI:
                case SYS_MONITOR_CMD_RUN_DIAG:
                    // For now, just respond with "not implemented"
                    resp_msg.type = SYS_MONITOR_RESP_ERROR;
                    resp_msg.status = ESP_ERR_NOT_SUPPORTED;
                    resp_msg.data = NULL;
                    resp_msg.data_len = 0;
                    xQueueSend(system_monitor_resp_queue, &resp_msg, portMAX_DELAY);
                    break;
    
                default:
                    ESP_LOGW(TAG, "Unknown command received: %d", cmd_msg.cmd);
                    resp_msg.type = SYS_MONITOR_RESP_ERROR;
                    resp_msg.status = ESP_ERR_INVALID_ARG;
                    resp_msg.data = NULL;
                    resp_msg.data_len = 0;
                    xQueueSend(system_monitor_resp_queue, &resp_msg, portMAX_DELAY);
                    break;
            }
        }

        #ifdef TEST_WDT_HANG
        static uint32_t startup_time = 0;
        if (startup_time == 0) {
            startup_time = esp_timer_get_time() / 1000; // Convert to ms
        }
        if ((esp_timer_get_time() / 1000) - startup_time > 10000) { // After 10s
            ESP_LOGW(TAG, "Simulating task hang...");
            vTaskDelay(pdMS_TO_TICKS(5000)); // Delay without WDT feed
        }
        #endif
        
        // Monitor heap
        uint32_t free_heap = esp_get_free_heap_size();
        if (free_heap < 10000) { // Example threshold
            ESP_LOGW(TAG, "Low heap memory: %lu bytes", free_heap);
            xEventGroupSetBits(system_event_group, SYSTEM_EVENT_ERROR);
        }

        // Monitor tasks
        TaskStatus_t *task_status_array;
        UBaseType_t task_count;
        uint32_t total_runtime;
        
        task_count = uxTaskGetNumberOfTasks();
        task_status_array = pvPortMalloc(task_count * sizeof(TaskStatus_t));
        
        if (task_status_array != NULL) {
            task_count = uxTaskGetSystemState(task_status_array, task_count, &total_runtime);
            
            // Log task statistics
            for (int i = 0; i < task_count; i++) {
                ESP_LOGD(TAG, "Task: %s, State: %s, Priority: %d",
                         task_status_array[i].pcTaskName,
                         get_task_state_name(task_status_array[i].eCurrentState),
                         task_status_array[i].uxCurrentPriority);
            }
            
            vPortFree(task_status_array);
        }

        // Wait for the next monitoring cycle
        vTaskDelayUntil(&last_wake_time, check_interval);
    }
}

// Get task state name
const char *get_task_state_name(eTaskState state) {
    switch (state) {
        case eRunning: return "Running";
        case eReady: return "Ready";
        case eBlocked: return "Blocked";
        case eSuspended: return "Suspended";
        case eDeleted: return "Deleted";
        default: return "Unknown";
    }
}

// Initialize task manager
esp_err_t task_manager_init(void) {
    ESP_LOGI(TAG, "Initializing task manager");
    
    // Create system event group
    system_event_group = xEventGroupCreate();
    if (system_event_group == NULL) {
        ESP_LOGE(TAG, "Failed to create system event group");
        return ESP_FAIL;
    }

    return ESP_OK;
}

// Create system monitor task
esp_err_t create_system_monitor_task(void) {
    BaseType_t ret = xTaskCreate(
        system_monitor_task,
        "system_monitor",
        TASK_STACK_SIZE_SYSTEM_MONITOR,
        NULL,
        TASK_PRIORITY_SYSTEM_MONITOR,
        &system_monitor_handle
    );

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create system monitor task");
        return ESP_FAIL;
    }

    return ESP_OK;
}

// Get system event group handle
EventGroupHandle_t get_system_event_group(void) {
    return system_event_group;
}

// Get task status
esp_err_t get_task_status(TaskHandle_t task_handle, task_status_t *status) {
    if (status == NULL || task_handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    TaskStatus_t task_details;
    vTaskGetInfo(task_handle, &task_details, pdTRUE, eInvalid);

    status->handle = task_handle;
    status->healthy = (task_details.eCurrentState != eSuspended && 
                      task_details.eCurrentState != eDeleted);
    status->execution_count = task_details.ulRunTimeCounter;
    status->wdt_subscribed = (esp_task_wdt_status(task_handle) == ESP_OK);

    return ESP_OK;
}
