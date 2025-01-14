#include <string.h>
#include "esp_log.h"
#include "esp_system.h"
#include "task_manager.h"
#include "esp_timer.h"
#include "esp_task_wdt.h"
#include "system_monitor_msg.h"
#include "system_manager.h"
#include "freertos/task.h"
#include "esp_cpu.h"

static const char *TAG = "task_manager";

// System event group
static EventGroupHandle_t system_event_group = NULL;

// Task handles
static TaskHandle_t system_monitor_handle = NULL;

// Enhanced task monitoring
typedef struct {
    TaskHandle_t handle;
    uint32_t last_runtime;
    uint32_t total_runtime;
    UBaseType_t stack_hwm;
    TaskStatus_t status;
    uint32_t last_error_code;
    uint64_t last_active_time;
} task_monitor_data_t;

#define MAX_MONITORED_TASKS 10
static task_monitor_data_t monitored_tasks[MAX_MONITORED_TASKS];
static size_t num_monitored_tasks = 0;

// Internal monitoring interval
static uint32_t monitor_interval_ms = 1000;

// Forward declarations for internal functions
static void update_task_statistics(void);
static void check_task_health(void);
static bool is_task_healthy(const task_monitor_data_t *task_data);

// System monitor task implementation
static void system_monitor_task(void *pvParameters) {
    TickType_t last_wake_time = xTaskGetTickCount();
    sys_monitor_cmd_msg_t cmd_msg;
    sys_monitor_resp_msg_t resp_msg;
    system_config_t sys_config;

    ESP_LOGI(TAG, "Enhanced system monitor task started");

    // Subscribe to watchdog
    ESP_ERROR_CHECK(esp_task_wdt_add(NULL));

    // Initial task statistics update
    update_task_statistics();

    while (1) {
        // Reset watchdog
        ESP_ERROR_CHECK(esp_task_wdt_reset());

        // Process incoming commands
        if (xQueueReceive(system_monitor_cmd_queue, &cmd_msg, 0) == pdTRUE) {
            switch (cmd_msg.cmd) {
                case SYS_MONITOR_CMD_GET_HEAP: {
                    resp_msg.type = SYS_MONITOR_RESP_HEAP;
                    resp_msg.status = ESP_OK;
                    size_t data_size = 3 * sizeof(uint32_t);
                    uint32_t *heap_info = malloc(data_size);
                    if (heap_info != NULL) {
                        heap_info[0] = esp_get_free_heap_size();
                        heap_info[1] = esp_get_minimum_free_heap_size();
                        heap_info[2] = heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT);
                        resp_msg.data = heap_info;
                        resp_msg.data_len = data_size;
                    } else {
                        resp_msg.status = ESP_ERR_NO_MEM;
                        resp_msg.data = NULL;
                        resp_msg.data_len = 0;
                    }
                    xQueueSend(system_monitor_resp_queue, &resp_msg, portMAX_DELAY);
                    break;
                }

                case SYS_MONITOR_CMD_GET_TASKS: {
                    resp_msg.type = SYS_MONITOR_RESP_TASKS;
                    resp_msg.status = ESP_OK;

                    size_t data_size = num_monitored_tasks * sizeof(task_status_t);
                    task_status_t *task_info = malloc(data_size);
                    if (task_info != NULL) {
                        for (size_t i = 0; i < num_monitored_tasks; i++) {
                            get_task_status(monitored_tasks[i].handle, &task_info[i]);
                        }
                        resp_msg.data = task_info;
                        resp_msg.data_len = data_size;
                    } else {
                        resp_msg.status = ESP_ERR_NO_MEM;
                        resp_msg.data = NULL;
                        resp_msg.data_len = 0;
                    }
                    xQueueSend(system_monitor_resp_queue, &resp_msg, portMAX_DELAY);
                    break;
                }

                case SYS_MONITOR_CMD_GET_WIFI: {
                    // Implemented in network manager
                    resp_msg.type = SYS_MONITOR_RESP_WIFI;
                    resp_msg.status = ESP_ERR_NOT_SUPPORTED;
                    resp_msg.data = NULL;
                    resp_msg.data_len = 0;
                    xQueueSend(system_monitor_resp_queue, &resp_msg, portMAX_DELAY);
                    break;
                }

                case SYS_MONITOR_CMD_RUN_DIAG: {
                    // Full system diagnostics
                    resp_msg.type = SYS_MONITOR_RESP_DIAG;
                    update_task_statistics();
                    check_task_health();
                    
                    system_diag_data_t *diag_data = malloc(sizeof(system_diag_data_t));
                    if (diag_data != NULL) {
                        esp_err_t ret = system_manager_get_diagnostics(diag_data);
                        resp_msg.status = ret;
                        resp_msg.data = diag_data;
                        resp_msg.data_len = sizeof(system_diag_data_t);
                    } else {
                        resp_msg.status = ESP_ERR_NO_MEM;
                        resp_msg.data = NULL;
                        resp_msg.data_len = 0;
                    }
                    xQueueSend(system_monitor_resp_queue, &resp_msg, portMAX_DELAY);
                    break;
                }

                case SYS_MONITOR_CMD_SET_INTERVAL: {
                    if (cmd_msg.data && cmd_msg.data_len == sizeof(uint32_t)) {
                        uint32_t new_interval = *(uint32_t*)cmd_msg.data;
                        if (new_interval >= 100 && new_interval <= 60000) {
                            monitor_interval_ms = new_interval;
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

        // Regular monitoring activities
        update_task_statistics();
        check_task_health();

        // System config-based checks
        if (system_manager_load_system_config(&sys_config) == ESP_OK) {
            uint32_t free_heap = esp_get_free_heap_size();
            if (free_heap < sys_config.min_heap_threshold) {
                ESP_LOGW(TAG, "Low heap memory: %lu bytes", free_heap);
                xEventGroupSetBits(system_event_group, SYSTEM_EVENT_LOW_MEMORY);
            }

            // Check stack watermarks
            for (size_t i = 0; i < num_monitored_tasks; i++) {
                if (monitored_tasks[i].stack_hwm < sys_config.stack_hwm_threshold) {
                    ESP_LOGW(TAG, "Low stack for task %s: %u bytes", 
                            monitored_tasks[i].status.pcTaskName,
                            monitored_tasks[i].stack_hwm);
                    xEventGroupSetBits(system_event_group, SYSTEM_EVENT_STACK_WARNING);
                }
            }
        }

        // Wait for next monitoring cycle
        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(monitor_interval_ms));
    }
}

// Internal helper functions
static void update_task_statistics(void) {
    TaskStatus_t *task_array = pvPortMalloc(uxTaskGetNumberOfTasks() * sizeof(TaskStatus_t));
    if (task_array != NULL) {
        uint32_t total_runtime;
        size_t task_count = uxTaskGetSystemState(task_array, 
                                               uxTaskGetNumberOfTasks(), 
                                               &total_runtime);
        
        // Update monitored tasks
        for (size_t i = 0; i < task_count && i < MAX_MONITORED_TASKS; i++) {
            monitored_tasks[i].handle = task_array[i].xHandle;
            monitored_tasks[i].last_runtime = task_array[i].ulRunTimeCounter;
            monitored_tasks[i].stack_hwm = uxTaskGetStackHighWaterMark(task_array[i].xHandle);
            memcpy(&monitored_tasks[i].status, &task_array[i], sizeof(TaskStatus_t));
            monitored_tasks[i].last_active_time = esp_timer_get_time();
        }
        num_monitored_tasks = (task_count < MAX_MONITORED_TASKS) ? task_count : MAX_MONITORED_TASKS;
        
        vPortFree(task_array);
    }
}

static void check_task_health(void) {
    for (size_t i = 0; i < num_monitored_tasks; i++) {
        if (!is_task_healthy(&monitored_tasks[i])) {
            ESP_LOGW(TAG, "Task %s health check failed", 
                    monitored_tasks[i].status.pcTaskName);
            xEventGroupSetBits(system_event_group, SYSTEM_EVENT_TASK_OVERRUN);
        }
    }
}

static bool is_task_healthy(const task_monitor_data_t *task_data) {
    if (task_data == NULL || task_data->handle == NULL) {
        return false;
    }

    // Check if it's a system task
    const char* system_tasks[] = {"ipc0", "ipc1", "esp_timer"};
    for (int i = 0; i < sizeof(system_tasks)/sizeof(char*); i++) {
        if (strcmp(task_data->status.pcTaskName, system_tasks[i]) == 0) {
            // For system tasks, Suspended state is normal
            // Only consider them unhealthy if deleted or invalid state
            return (task_data->status.eCurrentState != eDeleted && 
                    task_data->status.eCurrentState < eInvalid);
        }
    }

    // For regular tasks, check for unhealthy states
    if (task_data->status.eCurrentState == eDeleted || 
        task_data->status.eCurrentState >= eInvalid) {
        ESP_LOGW(TAG, "Task %s in unhealthy state: %d", 
                 task_data->status.pcTaskName,
                 task_data->status.eCurrentState);
        return false;
    }

    // Check if task has been active recently (only for non-system tasks)
    int64_t time_since_active = esp_timer_get_time() - task_data->last_active_time;
    if (time_since_active > (monitor_interval_ms * 3 * 1000)) {
        ESP_LOGW(TAG, "Task %s inactive for too long: %lld ms", 
                 task_data->status.pcTaskName,
                 time_since_active / 1000);
        return false;
    }

    return true;
}

// Public API implementations
esp_err_t task_manager_init(void) {
    ESP_LOGI(TAG, "Initializing task manager");
    
    system_event_group = xEventGroupCreate();
    if (system_event_group == NULL) {
        ESP_LOGE(TAG, "Failed to create system event group");
        return ESP_FAIL;
    }

    // Initialize monitoring data
    memset(monitored_tasks, 0, sizeof(monitored_tasks));
    num_monitored_tasks = 0;

    return ESP_OK;
}

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

esp_err_t get_task_status(TaskHandle_t task_handle, task_status_t *status) {
    if (status == NULL || task_handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    for (size_t i = 0; i < num_monitored_tasks; i++) {
        if (monitored_tasks[i].handle == task_handle) {
            status->handle = task_handle;
            status->healthy = is_task_healthy(&monitored_tasks[i]);
            status->execution_count = monitored_tasks[i].status.ulRunTimeCounter;
            status->wdt_subscribed = (esp_task_wdt_status(task_handle) == ESP_OK);
            status->last_wake_time = monitored_tasks[i].last_runtime;
            status->stack_hwm = monitored_tasks[i].stack_hwm;
            status->last_error_code = monitored_tasks[i].last_error_code;
            
            // Calculate runtime percentage
            if (monitored_tasks[i].total_runtime > 0) {
                status->runtime_percentage = (monitored_tasks[i].last_runtime * 100) / 
                                          monitored_tasks[i].total_runtime;
            } else {
                status->runtime_percentage = 0;
            }
            
            return ESP_OK;
        }
    }

    // If task not found in monitored tasks, get basic info
    TaskStatus_t task_details;
    vTaskGetInfo(task_handle, &task_details, pdTRUE, eInvalid);

    status->handle = task_handle;
    status->healthy = true;  // Assume healthy if not monitored
    status->execution_count = task_details.ulRunTimeCounter;
    status->wdt_subscribed = (esp_task_wdt_status(task_handle) == ESP_OK);
    status->last_wake_time = 0;
    status->stack_hwm = uxTaskGetStackHighWaterMark(task_handle);
    status->last_error_code = 0;
    status->runtime_percentage = 0;

    return ESP_OK;
}

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

EventGroupHandle_t get_system_event_group(void) {
    return system_event_group;
}

EventBits_t get_system_events_detailed(task_status_t *task_status, size_t max_tasks, size_t *num_tasks) {
    if (task_status != NULL && num_tasks != NULL) {
        *num_tasks = (num_monitored_tasks < max_tasks) ? num_monitored_tasks : max_tasks;
        
        // Copy current task status for each monitored task
        for (size_t i = 0; i < *num_tasks; i++) {
            get_task_status(monitored_tasks[i].handle, &task_status[i]);
        }
    }
    
    return xEventGroupGetBits(system_event_group);
}

// Helper function to update task error state
esp_err_t update_task_error(TaskHandle_t task_handle, uint32_t error_code) {
    if (task_handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    for (size_t i = 0; i < num_monitored_tasks; i++) {
        if (monitored_tasks[i].handle == task_handle) {
            monitored_tasks[i].last_error_code = error_code;
            if (error_code != 0) {
                xEventGroupSetBits(system_event_group, SYSTEM_EVENT_ERROR);
            }
            return ESP_OK;
        }
    }

    return ESP_ERR_NOT_FOUND;
}
