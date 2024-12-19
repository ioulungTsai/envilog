#pragma once

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

// Command types for system monitor
typedef enum {
    SYS_MONITOR_CMD_GET_HEAP,      // Get heap usage
    SYS_MONITOR_CMD_GET_TASKS,     // Get task statistics
    SYS_MONITOR_CMD_GET_WIFI,      // Get WiFi status
    SYS_MONITOR_CMD_RUN_DIAG,      // Run full diagnostics
    SYS_MONITOR_CMD_SET_INTERVAL   // Set monitoring interval
} sys_monitor_cmd_t;

// Response types from system monitor
typedef enum {
    SYS_MONITOR_RESP_HEAP,     // Heap usage response
    SYS_MONITOR_RESP_TASKS,    // Task statistics response
    SYS_MONITOR_RESP_WIFI,     // WiFi status response
    SYS_MONITOR_RESP_DIAG,     // Full diagnostics response
    SYS_MONITOR_RESP_ERROR     // Error response
} sys_monitor_resp_t;

// Command message structure
typedef struct {
    sys_monitor_cmd_t cmd;     // Command type
    void* data;                // Optional data pointer
    size_t data_len;          // Length of data if present
} sys_monitor_cmd_msg_t;

// Response message structure
typedef struct {
    sys_monitor_resp_t type;   // Response type
    esp_err_t status;         // Operation status
    void* data;               // Response data pointer
    size_t data_len;          // Length of response data
} sys_monitor_resp_msg_t;

// Queue handles (declared extern, defined in system_monitor.c)
extern QueueHandle_t system_monitor_cmd_queue;    // For incoming commands
extern QueueHandle_t system_monitor_resp_queue;   // For responses

// Queue lengths and sizes
#define SYS_MONITOR_CMD_QUEUE_LEN  10
#define SYS_MONITOR_RESP_QUEUE_LEN 10

// Function declarations
esp_err_t system_monitor_queue_init(void);
esp_err_t system_monitor_queue_deinit(void);

// Helper functions for sending commands and receiving responses
esp_err_t system_monitor_send_command(sys_monitor_cmd_msg_t* cmd, TickType_t wait_ticks);
esp_err_t system_monitor_get_response(sys_monitor_resp_msg_t* resp, TickType_t wait_ticks);
