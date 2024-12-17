#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_err.h"

// Task priorities
#define TASK_PRIORITY_SYSTEM_MONITOR    (tskIDLE_PRIORITY + 5)
#define TASK_PRIORITY_NETWORK           (tskIDLE_PRIORITY + 4)
#define TASK_PRIORITY_SENSOR            (tskIDLE_PRIORITY + 3)
#define TASK_PRIORITY_DATA_PROCESSING   (tskIDLE_PRIORITY + 2)

// Task stack sizes
#define TASK_STACK_SIZE_SYSTEM_MONITOR  4096
#define TASK_STACK_SIZE_NETWORK         4096
#define TASK_STACK_SIZE_SENSOR          4096
#define TASK_STACK_SIZE_DATA_PROCESSING 4096

// System events
typedef enum {
    SYSTEM_EVENT_WIFI_CONNECTED = BIT0,
    SYSTEM_EVENT_WIFI_DISCONNECTED = BIT1,
    SYSTEM_EVENT_SENSOR_DATA_READY = BIT2,
    SYSTEM_EVENT_ERROR = BIT3
} system_event_t;

// Task status structure
typedef struct {
    TaskHandle_t handle;
    uint32_t last_wake_time;
    uint32_t execution_count;
    bool healthy;
    bool wdt_subscribed;  
} task_status_t;

// Task initialization
esp_err_t task_manager_init(void);

// Task creation functions
esp_err_t create_system_monitor_task(void);
esp_err_t create_network_task(void);

// Task utilities
esp_err_t get_task_status(TaskHandle_t task_handle, task_status_t *status);
const char *get_task_state_name(eTaskState state);

// System events
EventGroupHandle_t get_system_event_group(void);
