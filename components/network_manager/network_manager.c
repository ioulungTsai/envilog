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
#include "esp_netif.h"
#include "builtin_led.h"

#define BOOT_CONNECTION_TIMEOUT_MS 12000  // 12 seconds for boot
#define WEB_CONNECTION_TIMEOUT_MS  8000   // 8 seconds for web interface

static const char *TAG = "network_manager";

// Network manager state
static TaskHandle_t network_task_handle = NULL;
static EventGroupHandle_t network_event_group = NULL;
static esp_timer_handle_t connection_timeout_timer = NULL;
static esp_netif_t *sta_netif = NULL;
static esp_netif_t *ap_netif = NULL;
static network_mode_t current_mode = NETWORK_MODE_STATION;
static bool is_provisioned = false;
static bool led_available = false;

// Boot vs Web operation context
typedef enum {
    WIFI_CONTEXT_BOOT,     // Boot-time connection attempt
    WIFI_CONTEXT_WEB,      // Web interface triggered attempt
    WIFI_CONTEXT_RUNTIME   // Runtime operation
} wifi_context_t;

static wifi_context_t current_context = WIFI_CONTEXT_BOOT;
static bool web_switch_in_progress = false;
static char web_switch_result_ip[16] = {0};

// Forward declarations
static void network_task(void *pvParameters);
static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
static void ip_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
static esp_err_t configure_station_mode(void);
static esp_err_t configure_ap_mode(void);
static esp_err_t check_provisioning_status(void);
static void connection_timeout_callback(void* arg);
static bool validate_stored_credentials(void);
static void update_status_led(led_status_t status);

// Helper function to safely update LED status
static void update_status_led(led_status_t status) {
    if (led_available) {
        esp_err_t ret = builtin_led_set_status(status);
        if (ret != ESP_OK) {
            ERROR_LOG_WARNING(TAG, ret, ERROR_CAT_HARDWARE, "Failed to update LED status");
        }
    }
}

esp_err_t network_manager_init(void)
{
    ESP_LOGI(TAG, "Initializing network manager with unified boot logic");

    // Create event group
    network_event_group = xEventGroupCreate();
    if (network_event_group == NULL) {
        ERROR_LOG_ERROR(TAG, ESP_FAIL, ERROR_CAT_SYSTEM, "Failed to create event group");
        return ESP_FAIL;
    }

    // Initialize TCP/IP stack and create network interfaces
    ESP_ERROR_CHECK(esp_netif_init());
    sta_netif = esp_netif_create_default_wifi_sta();
    ap_netif = esp_netif_create_default_wifi_ap();
    assert(sta_netif);
    assert(ap_netif);

    // Initialize WiFi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Register event handlers
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &ip_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_AP_STAIPASSIGNED, &ip_event_handler, NULL));

    // Create connection timeout timer
    esp_timer_create_args_t timer_args = {
        .callback = connection_timeout_callback,
        .name = "connection_timeout"
    };
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &connection_timeout_timer));

    // Check provisioning status
    check_provisioning_status();

    // Initialize status LED (optional - system works without it)
    if (builtin_led_init() == ESP_OK) {
        led_available = true;
        ESP_LOGI(TAG, "Status LED initialized successfully");
    } else {
        ERROR_LOG_WARNING(TAG, ESP_FAIL, ERROR_CAT_HARDWARE,
            "Status LED initialization failed - continuing without LED status");
        led_available = false;
    }

    return ESP_OK;
}

esp_err_t network_manager_start(void)
{
    BaseType_t ret = xTaskCreate(network_task, "network_task", TASK_STACK_SIZE_NETWORK,
                                NULL, TASK_PRIORITY_NETWORK, &network_task_handle);

    if (ret != pdPASS) {
        ERROR_LOG_ERROR(TAG, ESP_FAIL, ERROR_CAT_SYSTEM, "Failed to create network task");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Network manager started");
    return ESP_OK;
}

static void network_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Network task starting - implementing boot decision logic");
    ESP_ERROR_CHECK(esp_task_wdt_add(NULL));

    // Show dim white during boot decision
    update_status_led(LED_STATUS_BOOT);

    // === BOOT DECISION LOGIC ===
    if (!validate_stored_credentials()) {
        ESP_LOGI(TAG, "No valid credentials → Immediate AP mode");
        current_mode = NETWORK_MODE_AP;
        configure_ap_mode();
    } else {
        ESP_LOGI(TAG, "Valid credentials found → Attempting Station mode");
        current_context = WIFI_CONTEXT_BOOT;
        current_mode = NETWORK_MODE_STATION;
        
        // Start 12-second boot timeout
        esp_timer_start_once(connection_timeout_timer, BOOT_CONNECTION_TIMEOUT_MS * 1000);
        configure_station_mode();
    }

    // Main network monitoring loop
    while (1) {
        ESP_ERROR_CHECK(esp_task_wdt_reset());
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    switch (event_id) {
        case WIFI_EVENT_STA_START:
            ESP_LOGI(TAG, "Station started - attempting connection");
            esp_wifi_connect();
            break;

        case WIFI_EVENT_STA_CONNECTED:
            ESP_LOGI(TAG, "WiFi connected - waiting for IP");
            break;

        case WIFI_EVENT_STA_DISCONNECTED: {
            wifi_event_sta_disconnected_t* disconnected = (wifi_event_sta_disconnected_t*) event_data;
            
            // Stop any running timeout timer
            esp_timer_stop(connection_timeout_timer);
            
            ESP_LOGI(TAG, "WiFi disconnected (reason: %d)", disconnected->reason);
            
            if (current_context == WIFI_CONTEXT_BOOT) {
                // Boot-time disconnect
                if (disconnected->reason == WIFI_REASON_NO_AP_FOUND) {
                    ESP_LOGI(TAG, "Network not found → Immediate AP mode");
                } else {
                    ESP_LOGI(TAG, "Boot connection failed → AP mode");
                }
                current_mode = NETWORK_MODE_AP;
                configure_ap_mode();
                
            } else if (current_context == WIFI_CONTEXT_WEB) {
                // Web interface switch failed
                ESP_LOGI(TAG, "Web station switch failed → Stay in AP mode");
                web_switch_in_progress = false;
                memset(web_switch_result_ip, 0, sizeof(web_switch_result_ip));
                current_mode = NETWORK_MODE_AP;
                configure_ap_mode();
                
            } else {
                // Runtime disconnect → immediate AP fallback
                ESP_LOGI(TAG, "Runtime connection lost → Immediate AP fallback");
                current_mode = NETWORK_MODE_AP;
                configure_ap_mode();
            }
            
            xEventGroupClearBits(network_event_group, NETWORK_EVENT_WIFI_CONNECTED);
            xEventGroupSetBits(network_event_group, NETWORK_EVENT_WIFI_DISCONNECTED);
            break;
        }

        case WIFI_EVENT_AP_START:
            ESP_LOGI(TAG, "AP mode started: EnviLog");
            current_context = WIFI_CONTEXT_RUNTIME;
            xEventGroupSetBits(network_event_group, NETWORK_EVENT_AP_STARTED);
            // Update LED to AP mode
            update_status_led(LED_STATUS_AP_MODE);
            break;

        case WIFI_EVENT_AP_STOP:
            ESP_LOGI(TAG, "AP mode stopped");
            xEventGroupClearBits(network_event_group, NETWORK_EVENT_AP_STARTED);
            break;

        case WIFI_EVENT_AP_STACONNECTED: {
            wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
            ESP_LOGI(TAG, "Station connected to AP: " MACSTR, MAC2STR(event->mac));
            break;
        }
    }
}

static void ip_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP address: " IPSTR, IP2STR(&event->ip_info.ip));
        
        // Stop timeout timer
        esp_timer_stop(connection_timeout_timer);
        
        // Store IP for web interface response
        if (current_context == WIFI_CONTEXT_WEB) {
            snprintf(web_switch_result_ip, sizeof(web_switch_result_ip), 
                     IPSTR, IP2STR(&event->ip_info.ip));
            web_switch_in_progress = false;
        }
        
        current_mode = NETWORK_MODE_STATION;
        current_context = WIFI_CONTEXT_RUNTIME;
        xEventGroupSetBits(network_event_group, NETWORK_EVENT_WIFI_CONNECTED);
        xEventGroupClearBits(network_event_group, NETWORK_EVENT_WIFI_DISCONNECTED);

        // Update LED to Station mode
        update_status_led(LED_STATUS_STATION_MODE);
    }
}

static void connection_timeout_callback(void* arg)
{
    ESP_LOGI(TAG, "Connection timeout reached");
    
    if (current_context == WIFI_CONTEXT_BOOT) {
        ESP_LOGI(TAG, "12-second boot timeout → AP mode");
    } else if (current_context == WIFI_CONTEXT_WEB) {
        ESP_LOGI(TAG, "8-second web timeout → Stay in AP mode");
        web_switch_in_progress = false;
        memset(web_switch_result_ip, 0, sizeof(web_switch_result_ip));
    }
    
    // Force disconnect to trigger event handler
    esp_wifi_disconnect();
}

static esp_err_t configure_station_mode(void)
{
    network_config_t net_cfg;
    ESP_ERROR_CHECK(system_manager_load_network_config(&net_cfg));

    wifi_config_t wifi_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {.capable = true, .required = false},
        },
    };
    
    strlcpy((char*)wifi_config.sta.ssid, net_cfg.wifi_ssid, sizeof(wifi_config.sta.ssid));
    strlcpy((char*)wifi_config.sta.password, net_cfg.wifi_password, sizeof(wifi_config.sta.password));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    return ESP_OK;
}

static esp_err_t configure_ap_mode(void)
{
    ESP_LOGI(TAG, "Configuring AP mode");
    
    // Wait for WiFi to be in a stable state
    esp_err_t ret = esp_wifi_stop();
    if (ret != ESP_OK && ret != ESP_ERR_WIFI_NOT_STARTED) {
        ERROR_LOG_WARNING(TAG, ret, ERROR_CAT_NETWORK, "WiFi stop returned: %s", esp_err_to_name(ret));
    }
    
    // Small delay to ensure WiFi stack is ready
    vTaskDelay(pdMS_TO_TICKS(100));
    
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

    ret = esp_wifi_set_mode(WIFI_MODE_AP);
    if (ret != ESP_OK) {
        ERROR_LOG_ERROR(TAG, ret, ERROR_CAT_NETWORK, "Failed to set AP mode");
        return ret;
    }
    
    ret = esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
    if (ret != ESP_OK) {
        ERROR_LOG_ERROR(TAG, ret, ERROR_CAT_NETWORK, "Failed to set AP config");
        return ret;
    }
    
    ret = esp_wifi_start();
    if (ret != ESP_OK) {
        ERROR_LOG_ERROR(TAG, ret, ERROR_CAT_NETWORK, "Failed to start AP");
        return ret;
    }
    
    ESP_LOGI(TAG, "WiFi AP started successfully. SSID: %s", ENVILOG_AP_SSID);
    return ESP_OK;
}

static bool validate_stored_credentials(void)
{
    network_config_t config;
    if (system_manager_load_network_config(&config) != ESP_OK) {
        return false;
    }
    
    // Check for empty or placeholder values
    if (strlen(config.wifi_ssid) == 0 || 
        strcmp(config.wifi_ssid, "your-ssid") == 0 ||
        strcmp(config.wifi_password, "your-password") == 0) {
        return false;
    }
    
    return true;
}

static esp_err_t check_provisioning_status(void)
{
    is_provisioned = validate_stored_credentials();
    ESP_LOGI(TAG, "Provisioning status: %s", is_provisioned ? "Configured" : "Not configured");
    return ESP_OK;
}

// === PUBLIC API FUNCTIONS ===

bool network_manager_is_connected(void)
{
    return (xEventGroupGetBits(network_event_group) & NETWORK_EVENT_WIFI_CONNECTED) != 0;
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

EventGroupHandle_t network_manager_get_event_group(void)
{
    return network_event_group;
}

network_mode_t network_manager_get_mode(void)
{
    return current_mode;
}

bool network_manager_is_provisioned(void)
{
    return is_provisioned;
}

esp_err_t network_manager_web_station_switch(char* new_ip, size_t ip_len)
{
    if (new_ip == NULL || ip_len < 16) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (web_switch_in_progress) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Web interface triggered Station mode switch");
    
    // Initialize result
    memset(new_ip, 0, ip_len);
    memset(web_switch_result_ip, 0, sizeof(web_switch_result_ip));
    web_switch_in_progress = true;
    current_context = WIFI_CONTEXT_WEB;
    
    // Start 8-second timeout for web interface
    esp_timer_start_once(connection_timeout_timer, WEB_CONNECTION_TIMEOUT_MS * 1000);
    
    // Switch to Station mode
    esp_wifi_stop();
    vTaskDelay(pdMS_TO_TICKS(500));
    configure_station_mode();
    
    // Wait for result (up to 10 seconds total)
    for (int i = 0; i < 100; i++) {
        if (!web_switch_in_progress) {
            // Operation completed
            if (strlen(web_switch_result_ip) > 0) {
                strlcpy(new_ip, web_switch_result_ip, ip_len);
                ESP_LOGI(TAG, "Web station switch successful: %s", new_ip);
                return ESP_OK;
            } else {
                ESP_LOGI(TAG, "Web station switch failed");
                return ESP_FAIL;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    // Timeout
    web_switch_in_progress = false;
    ESP_LOGI(TAG, "Web station switch timeout");
    return ESP_ERR_TIMEOUT;
}
