#include <string.h>
#include "esp_log.h"
#include "esp_vfs.h"
#include "esp_http_server.h"
#include "cJSON.h"
#include "esp_system.h"
#include "esp_chip_info.h"
#include "esp_timer.h"
#include "esp_netif.h"
#include "http_server.h"
#include "network_manager.h"
#include <sys/stat.h>

static const char *TAG = "http_server";
static httpd_handle_t server = NULL;

// Function declarations for URI handlers
static esp_err_t system_info_handler(httpd_req_t *req);
static esp_err_t network_info_handler(httpd_req_t *req);
static esp_err_t static_file_handler(httpd_req_t *req);

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
    },
    {
        .uri = "/*",  // Catch-all handler for static files
        .method = HTTP_GET,
        .handler = static_file_handler,
        .user_ctx = NULL
    }
};

// System info handler implementation
static esp_err_t system_info_handler(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    cJSON_AddNumberToObject(root, "free_heap", esp_get_free_heap_size());
    cJSON_AddNumberToObject(root, "min_heap", esp_get_minimum_free_heap_size());
    cJSON_AddNumberToObject(root, "uptime_ms", esp_timer_get_time() / 1000);

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

// Network info handler implementation
static esp_err_t network_info_handler(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif) {
        esp_netif_ip_info_t ip_info;
        if (esp_netif_get_ip_info(netif, &ip_info) == ESP_OK) {
            char ip_str[16];
            snprintf(ip_str, sizeof(ip_str), IPSTR, IP2STR(&ip_info.ip));
            cJSON_AddStringToObject(root, "ip_address", ip_str);
        }
    }

    cJSON_AddBoolToObject(root, "connected", network_manager_is_connected());
    
    int8_t rssi;
    if (network_manager_get_rssi(&rssi) == ESP_OK) {
        cJSON_AddNumberToObject(root, "rssi", rssi);
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

// Static file handler
static esp_err_t static_file_handler(httpd_req_t *req)
{
    // Use a larger static buffer
    char filepath[CONFIG_SPIFFS_OBJ_NAME_LEN + ESP_VFS_PATH_MAX];
    FILE *fd = NULL;
    struct stat file_stat;
    
    const char *filename = req->uri;
    ESP_LOGI(TAG, "Requested URI: %s", filename);
    
    if (strcmp(filename, "/") == 0) {
        filename = "/index.html";
    }

    // Build full filepath
    int ret = snprintf(filepath, sizeof(filepath), "/www%s", filename);
    if (ret < 0 || ret >= sizeof(filepath)) {
        ESP_LOGE(TAG, "Filepath buffer too small");
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Trying to serve file: %s", filepath);

    if (stat(filepath, &file_stat) == -1) {
        ESP_LOGE(TAG, "Failed to stat file: %s", filepath);
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    fd = fopen(filepath, "r");
    if (!fd) {
        ESP_LOGE(TAG, "Failed to open file: %s", filepath);
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Serving file: %s (size: %ld bytes)", filepath, file_stat.st_size);

    // Set content type based on file extension
    const char *ext = strrchr(filename, '.');
    if (ext) {
        if (strcasecmp(ext, ".html") == 0) httpd_resp_set_type(req, "text/html");
        else if (strcasecmp(ext, ".css") == 0) httpd_resp_set_type(req, "text/css");
        else if (strcasecmp(ext, ".js") == 0) httpd_resp_set_type(req, "text/javascript");
        else if (strcasecmp(ext, ".ico") == 0) httpd_resp_set_type(req, "image/x-icon");
        else httpd_resp_set_type(req, "text/plain");
    }
    
    char *chunk = malloc(HTTP_CHUNK_SIZE);
    if (!chunk) {
        ESP_LOGE(TAG, "Memory allocation failed");
        fclose(fd);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    size_t chunksize;
    do {
        chunksize = fread(chunk, 1, HTTP_CHUNK_SIZE, fd);
        if (chunksize > 0) {
            if (httpd_resp_send_chunk(req, chunk, chunksize) != ESP_OK) {
                free(chunk);
                fclose(fd);
                ESP_LOGE(TAG, "File sending failed!");
                httpd_resp_send_500(req);
                return ESP_FAIL;
            }
        }
    } while (chunksize > 0);

    free(chunk);
    fclose(fd);
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

// Server initialization
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
    http_config.uri_match_fn = httpd_uri_match_wildcard;
    http_config.core_id = 0;
    http_config.task_priority = 1;
    http_config.stack_size = 8192;

    ESP_LOGI(TAG, "Starting HTTP server on port: %d", config->port);
    esp_err_t ret = httpd_start(&server, &http_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(ret));
        return ret;
    }

    // Register URI handlers
    for (size_t i = 0; i < sizeof(uri_handlers) / sizeof(uri_handlers[0]); i++) {
        ESP_LOGI(TAG, "Registering URI handler: %s", uri_handlers[i].uri);
        if (httpd_register_uri_handler(server, &uri_handlers[i]) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to register %s handler", uri_handlers[i].uri);
            http_server_stop();
            return ESP_FAIL;
        }
    }

    ESP_LOGI(TAG, "HTTP server started successfully");
    return ESP_OK;
}

// Server cleanup
esp_err_t http_server_stop(void)
{
    if (!server) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = httpd_stop(server);
    server = NULL;
    return ret;
}
