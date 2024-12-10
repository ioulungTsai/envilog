#include <string.h>
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "network_manager.h"
#include "task_manager.h"
#include "envilog_config.h"

static const char *TAG = "network_manager";

// Network manager state
static bool wifi_initialized = false;
static bool wifi_connected = false;
static TaskHandle_t network_task_handle = NULL;
static EventGroupHandle_t network_event_group = NULL;
static int retry_count = 0;

// Forward declarations
static void network_task(void *pvParameters);
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                             int32_t event_id, void* event_data);
static bool scan_for_ap(void);

esp_err_t network_manager_init(void)
{
    ESP_LOGI(TAG, "Initializing network manager");

    // Create event group
    network_event_group = xEventGroupCreate();
    if (network_event_group == NULL) {
        ESP_LOGE(TAG, "Failed to create event group");
        return ESP_FAIL;
    }

    // Initialize TCP/IP stack and create default WiFi station
    ESP_ERROR_CHECK(esp_netif_init());
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);

    // Initialize WiFi with default config
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    // Register event handlers
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, 
                                             &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, 
                                             &wifi_event_handler, NULL));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    wifi_initialized = true;
    return ESP_OK;
}

esp_err_t network_manager_start(void)
{
    if (!wifi_initialized) {
        ESP_LOGE(TAG, "Network manager not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    // Create network task
    BaseType_t ret = xTaskCreate(
        network_task,
        "network_task",
        TASK_STACK_SIZE_NETWORK,
        NULL,
        TASK_PRIORITY_NETWORK,
        &network_task_handle
    );

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create network task");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Network manager started");
    return ESP_OK;
}

static void network_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Network task starting");

    // Configure WiFi station with credentials
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = ENVILOG_WIFI_SSID,
            .password = ENVILOG_WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    // Task main loop
    while (1) {
        if (!wifi_connected) {
            if (scan_for_ap()) {
                ESP_LOGI(TAG, "Target AP found, attempting connection");
                esp_wifi_connect();
            } else {
                ESP_LOGW(TAG, "Target AP not found");
                vTaskDelay(pdMS_TO_TICKS(5000));  // Wait before retry
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1000));  // Regular check interval
    }
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                             int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                ESP_LOGI(TAG, "WiFi station started");
                break;

            case WIFI_EVENT_STA_DISCONNECTED: {
                wifi_event_sta_disconnected_t* event = (wifi_event_sta_disconnected_t*) event_data;
                ESP_LOGW(TAG, "Disconnected from AP (reason: %d)", event->reason);
                wifi_connected = false;
                retry_count = 0;  // Reset retry count on disconnect

                // Set event bits
                xEventGroupClearBits(network_event_group, NETWORK_EVENT_WIFI_CONNECTED);
                xEventGroupSetBits(network_event_group, NETWORK_EVENT_WIFI_DISCONNECTED);
                
                // Update system event group
                EventGroupHandle_t system_events = get_system_event_group();
                if (system_events != NULL) {
                    xEventGroupSetBits(system_events, SYSTEM_EVENT_WIFI_DISCONNECTED);
                    xEventGroupClearBits(system_events, SYSTEM_EVENT_WIFI_CONNECTED);
                }
                break;
            }

            default:
                break;
        }
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        wifi_connected = true;
        retry_count = 0;

        // Set event bits
        xEventGroupClearBits(network_event_group, NETWORK_EVENT_WIFI_DISCONNECTED);
        xEventGroupSetBits(network_event_group, NETWORK_EVENT_WIFI_CONNECTED);

        // Update system event group
        EventGroupHandle_t system_events = get_system_event_group();
        if (system_events != NULL) {
            xEventGroupSetBits(system_events, SYSTEM_EVENT_WIFI_CONNECTED);
            xEventGroupClearBits(system_events, SYSTEM_EVENT_WIFI_DISCONNECTED);
        }
    }
}

static bool scan_for_ap(void)
{
    ESP_LOGD(TAG, "Scanning for SSID: %s", ENVILOG_WIFI_SSID);
    
    wifi_scan_config_t scan_config = {
        .ssid = 0,
        .bssid = 0,
        .channel = 0,
        .show_hidden = true
    };

    ESP_ERROR_CHECK(esp_wifi_scan_start(&scan_config, true));

    uint16_t ap_count = 0;
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
    
    if (ap_count == 0) {
        ESP_LOGW(TAG, "No networks found");
        return false;
    }

    wifi_ap_record_t *ap_records = malloc(ap_count * sizeof(wifi_ap_record_t));
    if (ap_records == NULL) {
        ESP_LOGE(TAG, "Failed to malloc for AP records");
        return false;
    }

    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&ap_count, ap_records));

    bool found = false;
    for (int i = 0; i < ap_count; i++) {
        if (strcmp((char *)ap_records[i].ssid, ENVILOG_WIFI_SSID) == 0) {
            ESP_LOGD(TAG, "Found target AP: %s (RSSI: %d dBm, Channel: %d)", 
                     ap_records[i].ssid, ap_records[i].rssi, ap_records[i].primary);
            found = true;
            break;
        }
    }

    free(ap_records);
    return found;
}

EventGroupHandle_t network_manager_get_event_group(void)
{
    return network_event_group;
}

esp_err_t network_manager_get_rssi(int8_t *rssi)
{
    if (!wifi_connected || rssi == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    wifi_ap_record_t ap_info;
    esp_err_t ret = esp_wifi_sta_get_ap_info(&ap_info);
    if (ret == ESP_OK) {
        *rssi = ap_info.rssi;
    }
    return ret;
}

bool network_manager_is_connected(void)
{
    return wifi_connected;
}
