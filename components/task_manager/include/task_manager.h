#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_err.h"

// Task priorities
#define TASK_PRIORITY_SYSTEM_MONITOR    (tskIDLE_PRIORITY + 5)
#define TASK_PRIORITY_NETWORK           (tskIDLE_PRIORITY + 4)
#define TASK_PRIORITY_SENSOR            (tskIDLE_PRIORITY + 3)
#define TASK_PRIORITY_MQTT              (tskIDLE_PRIORITY + 3)
#define TASK_PRIORITY_DATA_PROCESSING   (tskIDLE_PRIORITY + 2)

// Task stack sizes
#define TASK_STACK_SIZE_SYSTEM_MONITOR  4096
#define TASK_STACK_SIZE_NETWORK         4096
#define TASK_STACK_SIZE_MQTT            4096
#define TASK_STACK_SIZE_SENSOR          4096
#define TASK_STACK_SIZE_DATA_PROCESSING 4096

// Monitoring configuration
#define MONITOR_MIN_INTERVAL_MS         100     // Minimum monitoring interval
#define MONITOR_MAX_INTERVAL_MS         60000   // Maximum monitoring interval
#define MONITOR_DEFAULT_INTERVAL_MS     1000    // Default monitoring interval

// Threshold definitions
#define STACK_HWM_WARNING_THRESHOLD     512     // Stack high water mark warning level (bytes)
#define TASK_INACTIVITY_THRESHOLD_MS    5000    // Task inactivity threshold (milliseconds)

// System events (enhanced)
typedef enum {
    SYSTEM_EVENT_WIFI_CONNECTED = BIT0,
    SYSTEM_EVENT_WIFI_DISCONNECTED = BIT1,
    SYSTEM_EVENT_SENSOR_DATA_READY = BIT2,
    SYSTEM_EVENT_ERROR = BIT3,
    SYSTEM_EVENT_LOW_MEMORY = BIT4,        // Low memory condition
    SYSTEM_EVENT_STACK_WARNING = BIT5,     // Stack usage warning
    SYSTEM_EVENT_TASK_OVERRUN = BIT6,      // Task execution time exceeded
    SYSTEM_EVENT_WDT_WARNING = BIT7        // Watchdog warning
} system_event_t;

// Task state definitions (for detailed reporting)
typedef enum {
    TASK_STATE_HEALTHY = 0,
    TASK_STATE_WARNING,
    TASK_STATE_CRITICAL,
    TASK_STATE_SUSPENDED,
    TASK_STATE_DELETED
} task_health_state_t;

// Enhanced task status structure
typedef struct {
    TaskHandle_t handle;                // Task handle
    uint32_t last_wake_time;           // Last wake timestamp
    uint32_t execution_count;          // Execution counter
    bool healthy;                      // Overall health status
    bool wdt_subscribed;              // Watchdog subscription status
    UBaseType_t stack_hwm;            // Stack high water mark
    uint32_t runtime_percentage;       // CPU usage percentage
    uint32_t last_error_code;         // Last recorded error
    task_health_state_t health_state; // Detailed health state
} task_status_t;

/**
 * @brief Initialize the task manager
 * 
 * Initializes the task management system, including event groups
 * and monitoring structures.
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t task_manager_init(void);

/**
 * @brief Create the system monitor task
 * 
 * Creates and starts the system monitoring task that tracks
 * system health and task statistics.
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t create_system_monitor_task(void);

/**
 * @brief Get detailed task status
 * 
 * Retrieves comprehensive status information for a specific task.
 * 
 * @param task_handle Handle of the task to query
 * @param status Pointer to task_status_t structure to fill
 * @return esp_err_t ESP_OK on success
 */
esp_err_t get_task_status(TaskHandle_t task_handle, task_status_t *status);

/**
 * @brief Get task state name
 * 
 * Converts a FreeRTOS task state to a human-readable string.
 * 
 * @param state FreeRTOS task state
 * @return const char* String representation of the state
 */
const char *get_task_state_name(eTaskState state);

/**
 * @brief Get system event group handle
 * 
 * Returns the handle to the system event group for event management.
 * 
 * @return EventGroupHandle_t Handle to the system event group
 */
EventGroupHandle_t get_system_event_group(void);

/**
 * @brief Get detailed system events with task status
 * 
 * Retrieves current system events and detailed status for all monitored tasks.
 * 
 * @param task_status Array to store task status information
 * @param max_tasks Maximum number of tasks to report
 * @param num_tasks Actual number of tasks reported
 * @return EventBits_t Current system event bits
 */
EventBits_t get_system_events_detailed(task_status_t *task_status, size_t max_tasks, size_t *num_tasks);

/**
 * @brief Update task error state
 * 
 * Records an error code for a specific task and updates system event flags.
 * 
 * @param task_handle Handle of the task reporting the error
 * @param error_code Error code to record
 * @return esp_err_t ESP_OK on success
 */
esp_err_t update_task_error(TaskHandle_t task_handle, uint32_t error_code);
