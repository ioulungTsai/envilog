#include "system_monitor_msg.h"
#include "esp_log.h"
#include "error_handler.h"

static const char *TAG = "sys_monitor_msg";

// Define the queue handles
QueueHandle_t system_monitor_cmd_queue = NULL;
QueueHandle_t system_monitor_resp_queue = NULL;

esp_err_t system_monitor_queue_init(void) {
    // Create command queue
    system_monitor_cmd_queue = xQueueCreate(SYS_MONITOR_CMD_QUEUE_LEN, 
                                          sizeof(sys_monitor_cmd_msg_t));
    if (system_monitor_cmd_queue == NULL) {
        ERROR_LOG_ERROR(TAG, ESP_ERR_NO_MEM, ERROR_CAT_SYSTEM,
            "Failed to create command queue");
        return ESP_ERR_NO_MEM;
    }

    // Create response queue
    system_monitor_resp_queue = xQueueCreate(SYS_MONITOR_RESP_QUEUE_LEN,
                                           sizeof(sys_monitor_resp_msg_t));
    if (system_monitor_resp_queue == NULL) {
        ERROR_LOG_ERROR(TAG, ESP_ERR_NO_MEM, ERROR_CAT_SYSTEM,
            "Failed to create response queue");
        vQueueDelete(system_monitor_cmd_queue);  // Clean up if response queue creation fails
        system_monitor_cmd_queue = NULL;
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "System monitor queues initialized");
    return ESP_OK;
}

esp_err_t system_monitor_queue_deinit(void) {
    if (system_monitor_cmd_queue != NULL) {
        vQueueDelete(system_monitor_cmd_queue);
        system_monitor_cmd_queue = NULL;
    }
    
    if (system_monitor_resp_queue != NULL) {
        vQueueDelete(system_monitor_resp_queue);
        system_monitor_resp_queue = NULL;
    }

    ESP_LOGI(TAG, "System monitor queues deinitialized");
    return ESP_OK;
}

esp_err_t system_monitor_send_command(sys_monitor_cmd_msg_t* cmd, TickType_t wait_ticks) {
    if (cmd == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (system_monitor_cmd_queue == NULL) {
        ERROR_LOG_ERROR(TAG, ESP_ERR_INVALID_STATE, ERROR_CAT_SYSTEM,
            "Command queue not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (xQueueSend(system_monitor_cmd_queue, cmd, wait_ticks) != pdPASS) {
        ERROR_LOG_WARNING(TAG, ESP_ERR_TIMEOUT, ERROR_CAT_SYSTEM,
            "Failed to send command to queue (timeout)");
        return ESP_ERR_TIMEOUT;
    }

    return ESP_OK;
}

esp_err_t system_monitor_get_response(sys_monitor_resp_msg_t* resp, TickType_t wait_ticks) {
    if (resp == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (system_monitor_resp_queue == NULL) {
        ERROR_LOG_ERROR(TAG, ESP_ERR_INVALID_STATE, ERROR_CAT_SYSTEM,
            "Response queue not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (xQueueReceive(system_monitor_resp_queue, resp, wait_ticks) != pdPASS) {
        ERROR_LOG_WARNING(TAG, ESP_ERR_TIMEOUT, ERROR_CAT_SYSTEM,
            "No response received (timeout)");
        return ESP_ERR_TIMEOUT;
    }

    return ESP_OK;
}
