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

#define WIFI_CONNECTION_TIMEOUT_MS 12000  // 12 seconds

static const char *TAG = "network_manager";

// Network manager state
static TaskHandle_t network_task_handle = NULL;
static EventGroupHandle_t network_event_group = NULL;
static bool connection_timeout_reached = false;
static esp_timer_handle_t wifi_timeout_timer = NULL;
static esp_netif_t *sta_netif = NULL;
static esp_netif_t *ap_netif = NULL;
static network_mode_t current_mode = NETWORK_MODE_STATION;
static bool is_provisioned = false;

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
    ESP_LOGI(TAG, "Network task starting with dual-mode support");

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

        // Simple monitoring - no complex retry during boot
        
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
                // Stop any running timeout timer
                if (wifi_timeout_timer != NULL) {
                    esp_timer_stop(wifi_timeout_timer);
                }
                
                xEventGroupClearBits(network_event_group, NETWORK_EVENT_WIFI_CONNECTED);
                xEventGroupSetBits(network_event_group, NETWORK_EVENT_WIFI_DISCONNECTED);
                
                if (current_mode == NETWORK_MODE_STATION) {
                    ESP_LOGI(TAG, "WiFi disconnected, switching to AP mode");
                    configure_ap_mode();
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
                
                // Stop timeout timer on successful connection
                if (wifi_timeout_timer != NULL) {
                    esp_timer_stop(wifi_timeout_timer);
                }
                connection_timeout_reached = false;
                
                current_mode = NETWORK_MODE_STATION;
                xEventGroupSetBits(network_event_group, NETWORK_EVENT_WIFI_CONNECTED);
                xEventGroupClearBits(network_event_group, NETWORK_EVENT_WIFI_DISCONNECTED);
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

// Handle configuration updates
esp_err_t network_manager_update_config(void)
{
    network_config_t net_cfg;
    esp_err_t ret = system_manager_load_network_config(&net_cfg);
    if (ret != ESP_OK) {
        ERROR_LOG_ERROR(TAG, ret, ERROR_CAT_CONFIG, "Failed to load new network config");
        return ret;
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

    return ESP_OK;
}

static esp_err_t check_provisioning_status(void) {
    network_config_t net_cfg;
    esp_err_t ret = system_manager_load_network_config(&net_cfg);
    
    if (ret == ESP_OK && strlen(net_cfg.wifi_ssid) > 0 &&
        strcmp(net_cfg.wifi_ssid, "your-ssid") != 0 &&
        strcmp(net_cfg.wifi_password, "your-password") != 0) {
        is_provisioned = true;
        ESP_LOGI(TAG, "Valid WiFi credentials found for SSID: %s", net_cfg.wifi_ssid);
    } else {
        is_provisioned = false;
        ESP_LOGI(TAG, "No valid WiFi credentials found");
    }
    
    return ret;
}

static void wifi_timeout_callback(void* arg) {
    connection_timeout_reached = true;
    ESP_LOGI(TAG, "WiFi connection timeout (12s), switching to AP mode");

    // Only switch if not already in AP mode
    if (current_mode != NETWORK_MODE_AP) {
        current_mode = NETWORK_MODE_AP;
        configure_ap_mode();
    } else {
        ESP_LOGI(TAG, "Already in AP mode, skipping switch");
    }
}

static esp_err_t configure_station_mode(void) {
    ESP_LOGI(TAG, "Configuring Station mode");
    
    connection_timeout_reached = false;
    
    // Create and start timeout timer
    if (wifi_timeout_timer == NULL) {
        esp_timer_create_args_t timer_args = {
            .callback = wifi_timeout_callback,
            .name = "wifi_timeout"
        };
        esp_timer_create(&timer_args, &wifi_timeout_timer);
    } else {
        // Stop any existing timer before starting new one
        esp_timer_stop(wifi_timeout_timer);
    }

    esp_timer_start_once(wifi_timeout_timer, WIFI_CONNECTION_TIMEOUT_MS * 1000);
    
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
