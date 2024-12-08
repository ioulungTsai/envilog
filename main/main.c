#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_err.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "envilog_config.h"

static const char *TAG = "envilog";

// Event group for WiFi connection
static EventGroupHandle_t wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static int s_retry_num = 0;

static void wifi_scan(void)
{
    wifi_scan_config_t scan_config = {
        .ssid = 0,
        .bssid = 0,
        .channel = 0,
        .show_hidden = true
    };

    ESP_LOGI(TAG, "Starting WiFi scan...");
    ESP_ERROR_CHECK(esp_wifi_scan_start(&scan_config, true));

    uint16_t ap_count = 0;
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
    ESP_LOGI(TAG, "Found %d access points:", ap_count);

    if (ap_count == 0) {
        ESP_LOGI(TAG, "No networks found");
        return;
    }

    wifi_ap_record_t *ap_records = malloc(ap_count * sizeof(wifi_ap_record_t));
    if (ap_records == NULL) {
        ESP_LOGE(TAG, "Failed to malloc for AP records");
        return;
    }

    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&ap_count, ap_records));

    for (int i = 0; i < ap_count; i++) {
        ESP_LOGI(TAG, "AP %2d:", i + 1);
        ESP_LOGI(TAG, "       SSID: %s", ap_records[i].ssid);
        ESP_LOGI(TAG, "       RSSI: %d dBm", ap_records[i].rssi);
        ESP_LOGI(TAG, "    Channel: %d", ap_records[i].primary);
        
        char *auth_mode = "UNKNOWN";
        switch(ap_records[i].authmode) {
            case WIFI_AUTH_OPEN:           auth_mode = "OPEN"; break;
            case WIFI_AUTH_WEP:            auth_mode = "WEP"; break;
            case WIFI_AUTH_WPA_PSK:        auth_mode = "WPA_PSK"; break;
            case WIFI_AUTH_WPA2_PSK:       auth_mode = "WPA2_PSK"; break;
            case WIFI_AUTH_WPA_WPA2_PSK:   auth_mode = "WPA_WPA2_PSK"; break;
            case WIFI_AUTH_WPA3_PSK:       auth_mode = "WPA3_PSK"; break;
            case WIFI_AUTH_WPA2_WPA3_PSK:  auth_mode = "WPA2_WPA3_PSK"; break;
            default: break;
        }
        ESP_LOGI(TAG, "   Auth Mode: %s", auth_mode);
        ESP_LOGI(TAG, "------------");
    }

    free(ap_records);
}

static void event_handler(void* arg, esp_event_base_t event_base,
                         int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "Connecting to SSID: %s", ENVILOG_WIFI_SSID);
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t* event = (wifi_event_sta_disconnected_t*) event_data;
        const char* reason_str;
        switch (event->reason) {
            case WIFI_REASON_AUTH_EXPIRE:
                reason_str = "Auth Expired";
                break;
            case WIFI_REASON_AUTH_FAIL:
                reason_str = "Auth Failed";
                break;
            case WIFI_REASON_NO_AP_FOUND:
                reason_str = "AP Not Found";
                break;
            case WIFI_REASON_ASSOC_FAIL:
                reason_str = "Association Failed";
                break;
            case WIFI_REASON_HANDSHAKE_TIMEOUT:
                reason_str = "Handshake Timeout";
                break;
            default:
                reason_str = "Unknown";
        }
        ESP_LOGW(TAG, "Disconnected - Reason: %s (%d)", reason_str, event->reason);
        
        if (s_retry_num < ENVILOG_WIFI_RETRY_NUM) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "Retry %d/%d connecting to WiFi...", 
                    s_retry_num, ENVILOG_WIFI_RETRY_NUM);
        } else {
            xEventGroupSetBits(wifi_event_group, WIFI_FAIL_BIT);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static esp_err_t wifi_connect(void)
{
    esp_err_t ret = ESP_OK;
    wifi_event_group = xEventGroupCreate();

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = ENVILOG_WIFI_SSID,
            .password = ENVILOG_WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };

    ESP_LOGI(TAG, "Setting WiFi configuration...");
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    
    ESP_LOGI(TAG, "Starting connection attempt...");
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Waiting for WiFi connection (timeout: %d ms)...", ENVILOG_WIFI_CONN_TIMEOUT_MS);
    EventBits_t bits = xEventGroupWaitBits(wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            pdMS_TO_TICKS(ENVILOG_WIFI_CONN_TIMEOUT_MS));

    // Check connection status
    wifi_config_t current_conf;
    esp_wifi_get_config(WIFI_IF_STA, &current_conf);
    ESP_LOGI(TAG, "Connection attempt details:");
    ESP_LOGI(TAG, "- SSID: %s", current_conf.sta.ssid);
    ESP_LOGI(TAG, "- Auth Mode: WPA2_PSK");
    ESP_LOGI(TAG, "- Password Length: %d", strlen((char*)current_conf.sta.password));

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Successfully connected to WiFi");
        ret = ESP_OK;
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGE(TAG, "Failed to connect to WiFi");
        ret = ESP_FAIL;
    } else {
        ESP_LOGE(TAG, "WiFi connection timeout - No response from AP");
        // Get current WiFi state
        wifi_ap_record_t ap_info;
        if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
            ESP_LOGI(TAG, "Last known AP state:");
            ESP_LOGI(TAG, "- RSSI: %d", ap_info.rssi);
            ESP_LOGI(TAG, "- Channel: %d", ap_info.primary);
        }
        ret = ESP_ERR_TIMEOUT;
    }

    return ret;
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

    // Initialize TCP/IP adapter
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);

    // Initialize WiFi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    // First scan for networks
    wifi_scan();

    // Register event handlers
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));

    // Then try to connect
    ret = wifi_connect();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi connection failed");
    }

    // Main loop
    while (1) {
        ESP_LOGW(TAG, "System running - WiFi Status: %s", 
             (ret == ESP_OK) ? "Connected" : "Disconnected");
        vTaskDelay(pdMS_TO_TICKS(5000));  // Log every 5 seconds
    }
}
