#include <string.h>
#include "esp_log.h"
#include "esp_event.h"
#include "mqtt_client.h"
#include "envilog_mqtt.h"
#include "network_manager.h"
#include "envilog_config.h"
#include "system_manager.h"
#include "task_manager.h"

static const char *TAG = "envilog_mqtt";

static EventGroupHandle_t mqtt_event_group = NULL;
static esp_mqtt_client_handle_t mqtt_client = NULL;
static bool immediate_retry = true;
static int retry_count = 0;

static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                             int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    int msg_id;

    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            retry_count = 0;         // Reset counter
            immediate_retry = true;  // Reset to immediate mode
            ESP_LOGI(TAG, "MQTT Connected - queued messages will be sent");
            msg_id = esp_mqtt_client_subscribe(event->client, ENVILOG_MQTT_TOPIC_STATUS, 0);
            ESP_LOGI(TAG, "Subscribed to status, msg_id=%d", msg_id);
            xEventGroupSetBits(mqtt_event_group, ENVILOG_MQTT_CONNECTED_BIT);
            xEventGroupClearBits(mqtt_event_group, ENVILOG_MQTT_DISCONNECTED_BIT);
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT Disconnected - messages will be queued");
            xEventGroupSetBits(mqtt_event_group, ENVILOG_MQTT_DISCONNECTED_BIT);
            xEventGroupClearBits(mqtt_event_group, ENVILOG_MQTT_CONNECTED_BIT);
            break;

        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "Message published successfully, msg_id=%d", event->msg_id);
            break;

        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "Received data on topic: %.*s", event->topic_len, event->topic);
            ESP_LOGI(TAG, "Data: %.*s", event->data_len, event->data);
            break;

       case MQTT_EVENT_ERROR:
           // Only log errors when WiFi is connected
           if (network_manager_is_connected()) {
               ESP_LOGW(TAG, "MQTT Error occurred");
               if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
                   ESP_LOGW(TAG, "Last error code reported from esp-tls: 0x%x", 
                       event->error_handle->esp_tls_last_esp_err);
               }
           }
           xEventGroupSetBits(mqtt_event_group, ENVILOG_MQTT_ERROR_BIT);
           break;

        default:
            ESP_LOGD(TAG, "Other event id: %d", event->event_id);
            break;
    }
}

static void mqtt_reconnect_task(void *pvParameters)
{
    bool was_connected = false;  // Track previous WiFi state
    
    while (1) {
        bool is_connected = network_manager_is_connected();
        
        // Handle WiFi state change
        if (was_connected && !is_connected) {
            // WiFi just disconnected
            esp_mqtt_client_stop(mqtt_client);
            xEventGroupClearBits(mqtt_event_group, ENVILOG_MQTT_CONNECTED_BIT);
            xEventGroupSetBits(mqtt_event_group, ENVILOG_MQTT_DISCONNECTED_BIT);
        }
        
        if (is_connected) {
            if (!envilog_mqtt_is_connected()) {
                if (immediate_retry && retry_count < ENVILOG_WIFI_RETRY_NUM) {
                    ESP_LOGI(TAG, "MQTT retry connecting (%d/%d)",
                            retry_count + 1, ENVILOG_WIFI_RETRY_NUM);
                    if (esp_mqtt_client_start(mqtt_client) != ESP_OK) {
                        retry_count++;
                    }
                } else if (retry_count == ENVILOG_WIFI_RETRY_NUM) {
                    ESP_LOGW(TAG, "MQTT failed after maximum retries, switching to periodic reconnection");
                    immediate_retry = false;
                    retry_count++;
                }
                vTaskDelay(pdMS_TO_TICKS(immediate_retry ? 
                    ENVILOG_MQTT_TIMEOUT_MS : ENVILOG_MQTT_RETRY_TIMEOUT_MS));
            }
        }
        
        was_connected = is_connected;
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

esp_err_t envilog_mqtt_init(void)
{
    ESP_LOGI(TAG, "Initializing MQTT client");

    mqtt_config_t mqtt_cfg;
    esp_err_t ret = system_manager_load_mqtt_config(&mqtt_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to load MQTT config");
        return ret;
    }

    mqtt_event_group = xEventGroupCreate();
    if (mqtt_event_group == NULL) {
        ESP_LOGE(TAG, "Failed to create event group");
        return ESP_FAIL;
    }

    esp_mqtt_client_config_t esp_mqtt_cfg = {
        .broker.address.uri = mqtt_cfg.broker_url,
        .credentials.client_id = mqtt_cfg.client_id,
        .session.keepalive = mqtt_cfg.keepalive,
        
        // Timeout settings
        .network.timeout_ms = mqtt_cfg.timeout_ms,
        .network.reconnect_timeout_ms = mqtt_cfg.retry_timeout_ms,

        // Outbox configuration
        .outbox.limit = ENVILOG_MQTT_OUTBOX_SIZE * 1024,
        
        // Session settings
        .session.disable_clean_session = false,
        
        // Last will message
        .session.last_will = {
            .topic = "/envilog/status",
            .msg = "offline",
            .qos = 1,
            .retain = 1
        }
    };

    mqtt_client = esp_mqtt_client_init(&esp_mqtt_cfg);
    if (mqtt_client == NULL) {
        ESP_LOGE(TAG, "Failed to initialize MQTT client");
        return ESP_FAIL;
    }

    ESP_ERROR_CHECK(esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID,
                                                 mqtt_event_handler, NULL));

    return ESP_OK;
}

esp_err_t envilog_mqtt_start(void)
{
    ESP_LOGI(TAG, "Starting MQTT Client");
    
    // Create reconnection task
    xTaskCreate(mqtt_reconnect_task, "mqtt_reconnect", TASK_STACK_SIZE_MQTT,
                NULL, TASK_PRIORITY_MQTT, NULL);

    return ESP_OK;
}

bool envilog_mqtt_is_connected(void)
{
    return (xEventGroupGetBits(mqtt_event_group) & ENVILOG_MQTT_CONNECTED_BIT) != 0;
}

EventGroupHandle_t envilog_mqtt_get_event_group(void)
{
    return mqtt_event_group;
}

esp_err_t envilog_mqtt_publish_status(const char *data, size_t len)
{
    if (!mqtt_client || !data) {
        return ESP_ERR_INVALID_ARG;
    }

    len = (len == 0) ? strlen(data) : len;

    int msg_id = esp_mqtt_client_publish(mqtt_client, ENVILOG_MQTT_TOPIC_STATUS,
                                       data, len,
                                       0,    // QoS 0 for frequent updates
                                       0);   // Don't retain

    if (msg_id < 0) {
        ESP_LOGW(TAG, "Failed to publish status update");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Published status update, msg_id=%d", msg_id);
    return ESP_OK;
}

esp_err_t envilog_mqtt_publish_diagnostic(const char *type, const char *data, size_t len)
{
    if (!mqtt_client || !type || !data) {
        return ESP_ERR_INVALID_ARG;
    }

    char full_topic[ENVILOG_MQTT_TOPIC_MAX_LEN];
    snprintf(full_topic, sizeof(full_topic), "%s/%s", 
             ENVILOG_MQTT_TOPIC_DIAGNOSTIC, type);

    len = (len == 0) ? strlen(data) : len;

    int msg_id = esp_mqtt_client_publish(mqtt_client, full_topic, data, len,
                                       1,    // QoS 1 to test queueing
                                       0);   // Don't retain

    if (msg_id < 0) {
        ESP_LOGW(TAG, "Failed to publish diagnostic data to topic %s", full_topic);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Published diagnostic data to %s, msg_id=%d", full_topic, msg_id);
    return ESP_OK;
}

esp_err_t envilog_mqtt_update_config(void)
{
    // Load new config
    mqtt_config_t mqtt_cfg;
    esp_err_t ret = system_manager_load_mqtt_config(&mqtt_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to load new MQTT config");
        return ret;
    }

    // Check client state
    if (mqtt_client == NULL) {
        ESP_LOGE(TAG, "MQTT client not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    // Only reconfigure if WiFi is connected and client is connected
    if (network_manager_is_connected() && envilog_mqtt_is_connected()) {
        esp_mqtt_client_stop(mqtt_client);
        esp_mqtt_client_destroy(mqtt_client);

        esp_mqtt_client_config_t esp_mqtt_cfg = {
            .broker.address.uri = mqtt_cfg.broker_url,
            .credentials.client_id = mqtt_cfg.client_id,
            .session.keepalive = mqtt_cfg.keepalive,
            .network.timeout_ms = mqtt_cfg.timeout_ms,
            .network.reconnect_timeout_ms = mqtt_cfg.retry_timeout_ms,
            .outbox.limit = ENVILOG_MQTT_OUTBOX_SIZE * 1024,
            .session.disable_clean_session = false,
            .session.last_will = {
                .topic = "/envilog/status",
                .msg = "offline",
                .qos = 1,
                .retain = 1
            }
        };

        mqtt_client = esp_mqtt_client_init(&esp_mqtt_cfg);
        if (mqtt_client == NULL) {
            ESP_LOGE(TAG, "Failed to initialize MQTT client");
            return ESP_FAIL;
        }

        ESP_ERROR_CHECK(esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID,
                                                     mqtt_event_handler, NULL));
    }

    // Reset retry mechanism regardless
    retry_count = 0;
    immediate_retry = true;

    ESP_LOGI(TAG, "MQTT configuration updated");
    return ESP_OK;
}
