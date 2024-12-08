#include <stdio.h>
#include "esp_log.h"
#include "esp_err.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "envilog";

static void wifi_scan(void)
{
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

    // Configure scan
    wifi_scan_config_t scan_config = {
        .ssid = 0,
        .bssid = 0,
        .channel = 0,
        .show_hidden = true
    };

    ESP_LOGI(TAG, "Starting WiFi scan...");
    ESP_ERROR_CHECK(esp_wifi_scan_start(&scan_config, true));

    // Get AP list
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

    // Print AP details
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

void app_main(void)
{
    // Initialize logging
    esp_log_level_set(TAG, ESP_LOG_INFO);
    ESP_LOGI(TAG, "Starting WiFi scan...");

    // Run scan
    wifi_scan();

    // Keep the app running
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
