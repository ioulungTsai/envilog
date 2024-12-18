#include <string.h>
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "network_manager.h"
#include "task_manager.h"
#include "envilog_config.h"
#include "esp_timer.h"
#include "esp_task_wdt.h"

static const char *TAG = "network_manager";

// Network manager state
static TaskHandle_t network_task_handle = NULL;
static EventGroupHandle_t network_event_group = NULL;
static bool immediate_retry = true;  // Added control flag
static int retry_count = 0;

// Forward declarations
static void network_task(void *pvParameters);
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                             int32_t event_id, void* event_data);

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

    return ESP_OK;
}

esp_err_t network_manager_start(void)
{
    BaseType_t ret = xTaskCreate(network_task, "network_task",
                                TASK_STACK_SIZE_NETWORK,
                                NULL, TASK_PRIORITY_NETWORK,
                                &network_task_handle);

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

    // Subscribe to WDT after WiFi init
    ESP_ERROR_CHECK(esp_task_wdt_add(NULL));

    const TickType_t retry_interval = pdMS_TO_TICKS(30000);  // 30 second interval

    while (1) {
        // Feed the watchdog
        ESP_ERROR_CHECK(esp_task_wdt_reset());

        #ifdef TEST_WDT_HANG
        static uint32_t startup_time = 0;
        if (startup_time == 0) {
            startup_time = esp_timer_get_time() / 1000;
        }
        if ((esp_timer_get_time() / 1000) - startup_time > 10000) {
            ESP_LOGW(TAG, "Simulating network task hang...");
            vTaskDelay(pdMS_TO_TICKS(5000));
        }
        #endif

        EventBits_t bits = xEventGroupGetBits(network_event_group);
        // Only attempt periodic reconnection if:
        // 1. Not connected
        // 2. Immediate retries are exhausted
        if (!(bits & NETWORK_EVENT_WIFI_CONNECTED) && !immediate_retry) {
            ESP_LOGI(TAG, "Attempting periodic reconnection");
            esp_wifi_connect();

            // Break long delay into smaller chunks with WDT feeds
            const TickType_t wdt_feed_interval = pdMS_TO_TICKS(2000); // Feed every 2s
            TickType_t remaining = retry_interval;
            while (remaining > 0) {
                TickType_t delay_time = (remaining > wdt_feed_interval) ? 
                                            wdt_feed_interval : remaining;
                vTaskDelay(delay_time);
                ESP_ERROR_CHECK(esp_task_wdt_reset());
                remaining -= delay_time;
            }
        } else {
            vTaskDelay(pdMS_TO_TICKS(1000));  // Regular status check interval
        }
    }
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                             int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                ESP_LOGI(TAG, "WiFi station started");
                esp_wifi_connect();
                break;

            case WIFI_EVENT_STA_DISCONNECTED:
                xEventGroupClearBits(network_event_group, NETWORK_EVENT_WIFI_CONNECTED);
                xEventGroupSetBits(network_event_group, NETWORK_EVENT_WIFI_DISCONNECTED);

                if (immediate_retry && retry_count < ENVILOG_WIFI_RETRY_NUM) {
                    ESP_LOGI(TAG, "Retry connecting to AP (%d/%d)", 
                            retry_count + 1, ENVILOG_WIFI_RETRY_NUM);
                    esp_wifi_connect();
                    retry_count++;
                } else if (retry_count == ENVILOG_WIFI_RETRY_NUM) {
                    ESP_LOGW(TAG, "Failed after maximum retries, switching to periodic reconnection");
                    immediate_retry = false;  // Switch to periodic mode
                    retry_count++;  // Prevent repeated warnings
                }
                break;
            
            case WIFI_EVENT_STA_CONNECTED:
                ESP_LOGI(TAG, "Connected to AP");
                break;
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP address: " IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(network_event_group, NETWORK_EVENT_WIFI_CONNECTED);
        xEventGroupClearBits(network_event_group, NETWORK_EVENT_WIFI_DISCONNECTED);
        retry_count = 0;         // Reset retry counter
        immediate_retry = true; // Reset to immediate retry mode
    }
}

esp_err_t network_manager_get_rssi(int8_t *rssi)
{
    if (!network_manager_is_connected() || rssi == NULL) {
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
    return (xEventGroupGetBits(network_event_group) & NETWORK_EVENT_WIFI_CONNECTED) != 0;
}
