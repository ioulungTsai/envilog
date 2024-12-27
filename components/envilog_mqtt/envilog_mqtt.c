#include <string.h>
#include "esp_log.h"
#include "esp_event.h"
#include "mqtt_client.h"
#include "envilog_mqtt.h"
#include "network_manager.h"
#include "envilog_config.h"

static const char *TAG = "envilog_mqtt";

static EventGroupHandle_t mqtt_event_group = NULL;
static esp_mqtt_client_handle_t mqtt_client = NULL;

static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                             int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;

    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT Connected");
            msg_id = esp_mqtt_client_subscribe(client, "/envilog/status", 0);
            ESP_LOGI(TAG, "Subscribed to status, msg_id=%d", msg_id);
            xEventGroupSetBits(mqtt_event_group, ENVILOG_MQTT_CONNECTED_BIT);
            xEventGroupClearBits(mqtt_event_group, ENVILOG_MQTT_DISCONNECTED_BIT);
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT Disconnected");
            xEventGroupSetBits(mqtt_event_group, ENVILOG_MQTT_DISCONNECTED_BIT);
            xEventGroupClearBits(mqtt_event_group, ENVILOG_MQTT_CONNECTED_BIT);
            break;

        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "Topic=%.*s Data=%.*s",
                event->topic_len, event->topic,
                event->data_len, event->data);
            break;

        case MQTT_EVENT_ERROR:
            ESP_LOGW(TAG, "MQTT Error");
            if (event->error_handle) {
                ESP_LOGW(TAG, "Error type: %d", event->error_handle->error_type);
            }
            xEventGroupSetBits(mqtt_event_group, ENVILOG_MQTT_ERROR_BIT);
            break;

        default:
            ESP_LOGD(TAG, "Other event id: %d", event->event_id);
            break;
    }
}

esp_err_t envilog_mqtt_init(void)
{
    ESP_LOGI(TAG, "Initializing MQTT client");

    mqtt_event_group = xEventGroupCreate();
    if (mqtt_event_group == NULL) {
        ESP_LOGE(TAG, "Failed to create event group");
        return ESP_FAIL;
    }

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = ENVILOG_MQTT_BROKER_URL,
        .credentials.client_id = ENVILOG_MQTT_CLIENT_ID,
        .session.keepalive = ENVILOG_MQTT_KEEPALIVE
    };

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
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
    return esp_mqtt_client_start(mqtt_client);
}

bool envilog_mqtt_is_connected(void)
{
    return (xEventGroupGetBits(mqtt_event_group) & ENVILOG_MQTT_CONNECTED_BIT) != 0;
}

EventGroupHandle_t envilog_mqtt_get_event_group(void)
{
    return mqtt_event_group;
}
