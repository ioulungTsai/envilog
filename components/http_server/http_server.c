#include <string.h>
#include "esp_log.h"
#include "esp_system.h"
#include "esp_chip_info.h"
#include "cJSON.h"
#include "http_server.h"
#include "esp_netif.h"
#include "esp_private/rtc_clk.h"

static const char *TAG = "http_server";
static httpd_handle_t server = NULL;

// System info handler
static esp_err_t system_info_handler(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    // Add system information using ESP-IDF APIs
    cJSON_AddNumberToObject(root, "free_heap", esp_get_free_heap_size());
    cJSON_AddNumberToObject(root, "min_heap", esp_get_minimum_free_heap_size());
    cJSON_AddNumberToObject(root, "uptime_ms", esp_timer_get_time() / 1000);

    // Get chip information using ESP-IDF API
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    
    cJSON *chip = cJSON_AddObjectToObject(root, "chip");
    if (chip) {
        cJSON_AddNumberToObject(chip, "cores", chip_info.cores);
        cJSON_AddNumberToObject(chip, "model", chip_info.model);
        cJSON_AddNumberToObject(chip, "revision", chip_info.revision);
        cJSON_AddStringToObject(chip, "features", esp_get_idf_version());
    }

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    
    if (!json_str) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_str, strlen(json_str));
    free(json_str);

    return ESP_OK;
}

// Network info handler
static esp_err_t network_info_handler(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    // Get IP info
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif) {
        esp_netif_ip_info_t ip_info;
        if (esp_netif_get_ip_info(netif, &ip_info) == ESP_OK) {
            char ip_str[16];
            snprintf(ip_str, sizeof(ip_str), IPSTR, IP2STR(&ip_info.ip));
            cJSON_AddStringToObject(root, "ip_address", ip_str);
        }
    }

    // Add WiFi status
    cJSON_AddBoolToObject(root, "connected", network_manager_is_connected());
    
    int8_t rssi;
    if (network_manager_get_rssi(&rssi) == ESP_OK) {
        cJSON_AddNumberToObject(root, "rssi", rssi);
    }

    // Convert to string and send
    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    
    if (!json_str) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_str, strlen(json_str));
    free(json_str);

    return ESP_OK;
}

// URI handlers configuration
static const httpd_uri_t uri_handlers[] = {
    {
        .uri = "/api/v1/system",
        .method = HTTP_GET,
        .handler = system_info_handler,
        .user_ctx = NULL
    },
    {
        .uri = "/api/v1/network",
        .method = HTTP_GET,
        .handler = network_info_handler,
        .user_ctx = NULL
    }
};

esp_err_t http_server_init(const http_server_config_t *config)
{
    if (server) {
        ESP_LOGW(TAG, "HTTP server already running");
        return ESP_ERR_INVALID_STATE;
    }

    httpd_config_t http_config = HTTPD_DEFAULT_CONFIG();
    http_config.server_port = config->port;
    http_config.max_open_sockets = config->max_clients;
    http_config.lru_purge_enable = true;

    // Configurations that resolved system monitor (Ctrl+T Ctrl+P) hang
    // Note: Root cause not yet confirmed, but these settings work:
    http_config.core_id = 0;        // Run on same core as system tasks
    http_config.task_priority = 1;   // Lower than system task priority
    http_config.stack_size = 8192;   // Sufficient stack for operations

    ESP_LOGI(TAG, "Starting HTTP server on port: %d", config->port);
    esp_err_t ret = httpd_start(&server, &http_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(ret));
        return ret;
    }

    // Register URI handlers
    for (size_t i = 0; i < sizeof(uri_handlers) / sizeof(uri_handlers[0]); i++) {
        if (httpd_register_uri_handler(server, &uri_handlers[i]) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to register %s handler", uri_handlers[i].uri);
            http_server_stop();
            return ESP_FAIL;
        }
    }

    ESP_LOGI(TAG, "HTTP server started successfully");
    return ESP_OK;
}

esp_err_t http_server_stop(void)
{
    if (!server) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = httpd_stop(server);
    server = NULL;
    return ret;
}
