#include <string.h>
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "network_manager.h"
#include "task_manager.h"
#include "envilog_config.h"
#include "esp_timer.h"
#include "esp_task_wdt.h"
#include "system_manager.h"
#include "error_handler.h"
#include "esp_mac.h"

#define EXTENDED_RETRY_COUNT      3       // Extended retries after fast phase
#define EXTENDED_RETRY_INTERVAL   30000   // 30 seconds between extended retries

static const char *TAG = "network_manager";

// Network manager state
static TaskHandle_t network_task_handle = NULL;
static EventGroupHandle_t network_event_group = NULL;
static bool immediate_retry = true;  // Added control flag
static int retry_count = 0;
static esp_netif_t *sta_netif = NULL;
static esp_netif_t *ap_netif = NULL;
static network_mode_t current_mode = NETWORK_MODE_STATION;
static bool is_provisioned = false;
static int extended_retry_count = 0;
static bool in_extended_retry_phase = false;
static int64_t last_extended_retry_time = 0;

// Forward declarations
static void network_task(void *pvParameters);
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                             int32_t event_id, void* event_data);
static esp_err_t configure_station_mode(void);
static esp_err_t configure_ap_mode(void);
static esp_err_t check_provisioning_status(void);

esp_err_t network_manager_init(void)
{
    ESP_LOGI(TAG, "Initializing network manager with dual-mode support");

    // Load network configuration
    network_config_t net_cfg;
    esp_err_t ret = system_manager_load_network_config(&net_cfg);
    if (ret != ESP_OK) {
        ERROR_LOG_ERROR(TAG, ret, ERROR_CAT_CONFIG, "Failed to load network config");
        return ret;
    }

    // Create event group
    network_event_group = xEventGroupCreate();
    if (network_event_group == NULL) {
        ERROR_LOG_ERROR(TAG, ESP_FAIL, ERROR_CAT_SYSTEM, "Failed to create event group");
        return ESP_FAIL;
    }

    // Initialize TCP/IP stack and create network interfaces
    ESP_ERROR_CHECK(esp_netif_init());
    
    // Create both station and AP interfaces
    sta_netif = esp_netif_create_default_wifi_sta();   // Your existing station
    ap_netif = esp_netif_create_default_wifi_ap();     // New: AP interface
    assert(sta_netif);
    assert(ap_netif);

    // Initialize WiFi with default config
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Register event handlers (add AP events to your existing ones)
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, 
                                             &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, 
                                             &wifi_event_handler, NULL));
    // New: AP event handler
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_AP_STAIPASSIGNED, 
                                             &wifi_event_handler, NULL));

    // Check if we have valid WiFi credentials
    check_provisioning_status();

    return ESP_OK;
}

esp_err_t network_manager_start(void)
{
    BaseType_t ret = xTaskCreate(network_task, "network_task",
                                TASK_STACK_SIZE_NETWORK,
                                NULL, TASK_PRIORITY_NETWORK,
                                &network_task_handle);

    if (ret != pdPASS) {
        ERROR_LOG_ERROR(TAG, ESP_FAIL, ERROR_CAT_SYSTEM, "Failed to create network task");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Network manager started");
    return ESP_OK;
}

static void network_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Network task starting with AP fallback support");

    // Subscribe to WDT after WiFi init
    ESP_ERROR_CHECK(esp_task_wdt_add(NULL));

    // Decide initial mode based on provisioning status
    if (is_provisioned) {
        ESP_LOGI(TAG, "Starting in Station mode (credentials found)");
        current_mode = NETWORK_MODE_STATION;
        configure_station_mode();
    } else {
        ESP_LOGI(TAG, "Starting in AP mode (no credentials)");
        current_mode = NETWORK_MODE_AP;
        configure_ap_mode();
    }

    while (1) {
        ESP_ERROR_CHECK(esp_task_wdt_reset());

        // Handle extended retry phase with proper timing
        if (current_mode == NETWORK_MODE_STATION && in_extended_retry_phase) {
            EventBits_t bits = xEventGroupGetBits(network_event_group);
            if (!(bits & NETWORK_EVENT_WIFI_CONNECTED)) {
                int64_t current_time = esp_timer_get_time();
                
                if (extended_retry_count < EXTENDED_RETRY_COUNT) {
                    // Check if enough time has passed since last retry
                    if (current_time - last_extended_retry_time > (EXTENDED_RETRY_INTERVAL * 1000)) {
                        extended_retry_count++;
                        ESP_LOGI(TAG, "Extended retry connecting to AP (%d/%d)", 
                                 extended_retry_count, EXTENDED_RETRY_COUNT);
                        esp_wifi_connect();
                        last_extended_retry_time = current_time;
                    }
                } else {
                    // Extended retries exhausted - switch to AP mode
                    ESP_LOGW(TAG, "All connection attempts failed, switching to AP fallback mode");
                    in_extended_retry_phase = false;
                    
                    esp_wifi_stop();
                    vTaskDelay(pdMS_TO_TICKS(100));
                    network_manager_start_ap_mode();
                }
            }
        }

        // Periodic reconnection logic (only when not in extended retry phase)
        if (current_mode == NETWORK_MODE_STATION && !in_extended_retry_phase) {
            EventBits_t bits = xEventGroupGetBits(network_event_group);
            if (!(bits & NETWORK_EVENT_WIFI_CONNECTED) && !immediate_retry) {
                ESP_LOGI(TAG, "Attempting periodic reconnection");
                esp_wifi_connect();

                const TickType_t wdt_feed_interval = pdMS_TO_TICKS(2000);
                TickType_t remaining = pdMS_TO_TICKS(30000);
                while (remaining > 0) {
                    TickType_t delay_time = (remaining > wdt_feed_interval) ? 
                                                wdt_feed_interval : remaining;
                    vTaskDelay(delay_time);
                    ESP_ERROR_CHECK(esp_task_wdt_reset());
                    remaining -= delay_time;
                }
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(1000));
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

                if (current_mode == NETWORK_MODE_STATION) {
                    if (immediate_retry && retry_count < ENVILOG_WIFI_RETRY_NUM) {
                        ESP_LOGI(TAG, "Fast retry connecting to AP (%d/%d)", 
                                retry_count + 1, ENVILOG_WIFI_RETRY_NUM);
                        esp_wifi_connect();
                        retry_count++;
                    } else if (retry_count == ENVILOG_WIFI_RETRY_NUM && !in_extended_retry_phase) {
                        // CRITICAL FIX: Only start extended retry phase once
                        ESP_LOGI(TAG, "Fast retries exhausted, starting extended retry phase");
                        immediate_retry = false;
                        in_extended_retry_phase = true;
                        extended_retry_count = 0;
                        last_extended_retry_time = esp_timer_get_time();
                    }
                    // Ignore subsequent disconnects when already in extended retry phase
                }
                break;
            
            case WIFI_EVENT_STA_CONNECTED:
                ESP_LOGI(TAG, "Connected to AP");
                break;

            case WIFI_EVENT_AP_START:
                ESP_LOGI(TAG, "WiFi AP started");
                current_mode = NETWORK_MODE_AP;
                xEventGroupSetBits(network_event_group, NETWORK_EVENT_AP_STARTED);
                break;

            case WIFI_EVENT_AP_STOP:
                ESP_LOGI(TAG, "WiFi AP stopped");
                xEventGroupSetBits(network_event_group, NETWORK_EVENT_AP_STOPPED);
                xEventGroupClearBits(network_event_group, NETWORK_EVENT_AP_STARTED);
                break;

            case WIFI_EVENT_AP_STACONNECTED: {
                wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
                ESP_LOGI(TAG, "Station " MACSTR " joined AP, AID=%d", 
                        MAC2STR(event->mac), event->aid);
                break;
            }

            case WIFI_EVENT_AP_STADISCONNECTED: {
                wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
                ESP_LOGI(TAG, "Station " MACSTR " left AP, AID=%d", 
                        MAC2STR(event->mac), event->aid);
                break;
            }
        }
    } else if (event_base == IP_EVENT) {
        switch (event_id) {
            case IP_EVENT_STA_GOT_IP: {
                ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
                ESP_LOGI(TAG, "Got IP address: " IPSTR, IP2STR(&event->ip_info.ip));
                
                current_mode = NETWORK_MODE_STATION;
                xEventGroupSetBits(network_event_group, NETWORK_EVENT_WIFI_CONNECTED);
                xEventGroupClearBits(network_event_group, NETWORK_EVENT_WIFI_DISCONNECTED);
                
                // Reset all retry state on successful connection
                retry_count = 0;
                extended_retry_count = 0;
                immediate_retry = true;
                in_extended_retry_phase = false;
                last_extended_retry_time = 0;
                
                ESP_LOGI(TAG, "Connection successful - all retry state reset");
                break;
            }

            case IP_EVENT_AP_STAIPASSIGNED: {
                ip_event_ap_staipassigned_t* event = (ip_event_ap_staipassigned_t*) event_data;
                ESP_LOGI(TAG, "AP assigned IP " IPSTR " to station", IP2STR(&event->ip));
                break;
            }
        }
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

EventGroupHandle_t network_manager_get_event_group(void)
{
    return network_event_group;
}

// Handle configuration updates
esp_err_t network_manager_update_config(void)
{
    network_config_t net_cfg;
    esp_err_t ret = system_manager_load_network_config(&net_cfg);
    if (ret != ESP_OK) {
        ERROR_LOG_ERROR(TAG, ret, ERROR_CAT_CONFIG, "Failed to load new network config");
        return ret;
    }

    // Check current WiFi mode before applying configuration
    wifi_mode_t current_wifi_mode;
    ret = esp_wifi_get_mode(&current_wifi_mode);
    if (ret != ESP_OK) {
        ERROR_LOG_ERROR(TAG, ret, ERROR_CAT_NETWORK, "Failed to get current WiFi mode");
        return ret;
    }

    // Only apply STA configuration if we're in STA or APSTA mode
    if (current_wifi_mode == WIFI_MODE_AP) {
        ESP_LOGI(TAG, "Currently in AP mode, storing STA config for later use");
        // Configuration is already saved to NVS, it will be applied when switching to STA mode
        return ESP_OK;
    }

    wifi_config_t wifi_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };

    strlcpy((char*)wifi_config.sta.ssid, net_cfg.wifi_ssid,
            sizeof(wifi_config.sta.ssid));
    strlcpy((char*)wifi_config.sta.password, net_cfg.wifi_password,
            sizeof(wifi_config.sta.password));

    // Apply new configuration
    ret = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if (ret != ESP_OK) {
        ERROR_LOG_ERROR(TAG, ret, ERROR_CAT_NETWORK, "Failed to set new WiFi config");
        return ret;
    }

    // Reconnect if currently connected
    if (network_manager_is_connected()) {
        esp_wifi_disconnect();
        esp_wifi_connect();
    }

    ESP_LOGI(TAG, "Network configuration updated successfully");
    return ESP_OK;
}

static esp_err_t check_provisioning_status(void) {
    network_config_t net_cfg;
    esp_err_t ret = system_manager_load_network_config(&net_cfg);
    
    if (ret == ESP_OK && strlen(net_cfg.wifi_ssid) > 0) {
        is_provisioned = true;
        ESP_LOGI(TAG, "WiFi credentials found for SSID: %s", net_cfg.wifi_ssid);
    } else {
        is_provisioned = false;
        ESP_LOGI(TAG, "No WiFi credentials found");
    }
    
    return ret;
}

static esp_err_t configure_station_mode(void) {
    ESP_LOGI(TAG, "Configuring Station mode");
    
    network_config_t net_cfg;
    ESP_ERROR_CHECK(system_manager_load_network_config(&net_cfg));

    wifi_config_t wifi_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };
    
    strlcpy((char*)wifi_config.sta.ssid, net_cfg.wifi_ssid, 
            sizeof(wifi_config.sta.ssid));
    strlcpy((char*)wifi_config.sta.password, net_cfg.wifi_password, 
            sizeof(wifi_config.sta.password));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    return ESP_OK;
}

static esp_err_t configure_ap_mode(void) {
    ESP_LOGI(TAG, "Configuring AP mode");
    
    wifi_config_t wifi_config = {
        .ap = {
            .ssid = ENVILOG_AP_SSID,
            .ssid_len = strlen(ENVILOG_AP_SSID),
            .channel = ENVILOG_AP_CHANNEL,
            .password = ENVILOG_AP_PASSWORD,
            .max_connection = ENVILOG_AP_MAX_CONNECTIONS,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    ESP_LOGI(TAG, "WiFi AP started. SSID: %s", ENVILOG_AP_SSID);
    return ESP_OK;
}

network_mode_t network_manager_get_mode(void) {
    return current_mode;
}

bool network_manager_is_provisioned(void) {
    return is_provisioned;
}

esp_err_t network_manager_start_ap_mode(void) {
    if (current_mode == NETWORK_MODE_AP) {
        ESP_LOGI(TAG, "Already in AP mode");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Switching to AP mode");
    current_mode = NETWORK_MODE_SWITCHING;
    
    esp_wifi_stop();
    vTaskDelay(pdMS_TO_TICKS(100));
    
    current_mode = NETWORK_MODE_AP;
    return configure_ap_mode();
}

esp_err_t network_manager_switch_to_station(void) {
    if (!is_provisioned) {
        ERROR_LOG_ERROR(TAG, ESP_ERR_INVALID_STATE, ERROR_CAT_CONFIG,
            "Cannot switch to station mode: no credentials");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (current_mode == NETWORK_MODE_STATION) {
        ESP_LOGI(TAG, "Already in Station mode");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Switching to Station mode");
    current_mode = NETWORK_MODE_SWITCHING;
    
    esp_wifi_stop();
    vTaskDelay(pdMS_TO_TICKS(100));
    
    current_mode = NETWORK_MODE_STATION;
    return configure_station_mode();
}
