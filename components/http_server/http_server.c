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
#include "system_manager.h"
#include <sys/stat.h>
#include "data_manager.h"
#include "esp_spiffs.h"
#include "error_handler.h"

static const char *TAG = "http_server";
static httpd_handle_t server = NULL;

/* Function Declarations */
static esp_err_t init_spiffs(void);
static esp_err_t system_info_handler(httpd_req_t *req);
static esp_err_t network_info_handler(httpd_req_t *req);
static esp_err_t static_file_handler(httpd_req_t *req);
static esp_err_t get_network_config_handler(httpd_req_t *req);
static esp_err_t get_mqtt_config_handler(httpd_req_t *req);
static esp_err_t update_network_config_handler(httpd_req_t *req);
static esp_err_t update_mqtt_config_handler(httpd_req_t *req);
static esp_err_t sensor_data_handler(httpd_req_t *req);

static esp_err_t init_spiffs(void) {
    ESP_LOGI(TAG, "Initializing SPIFFS");

    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/www",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = true
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ERROR_LOG_ERROR(TAG, ESP_FAIL, ERROR_CAT_STORAGE,
                "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ERROR_LOG_ERROR(TAG, ESP_ERR_NOT_FOUND, ERROR_CAT_STORAGE,
                "Failed to find SPIFFS partition");
        } else {
            ERROR_LOG_ERROR(TAG, ret, ERROR_CAT_STORAGE,
                "Failed to initialize SPIFFS");
        }
        return ret;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(NULL, &total, &used);
    if (ret != ESP_OK) {
        ERROR_LOG_ERROR(TAG, ret, ERROR_CAT_STORAGE,
            "Failed to get SPIFFS partition information");
        return ret;
    }

    ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    return ESP_OK;
}

/* URI Handler Configuration */
static const httpd_uri_t uri_handlers[] = {
    {
        .uri = "/api/v1/sensors/dht11",
        .method = HTTP_GET,
        .handler = sensor_data_handler,
        .user_ctx = NULL
    },
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
        .uri = "/api/v1/config/network",
        .method = HTTP_GET,
        .handler = get_network_config_handler,
        .user_ctx = NULL
    },
    {
        .uri = "/api/v1/config/network",
        .method = HTTP_POST,
        .handler = update_network_config_handler,
        .user_ctx = NULL
    },
    {
        .uri = "/api/v1/config/mqtt",
        .method = HTTP_GET,
        .handler = get_mqtt_config_handler,
        .user_ctx = NULL
    },
    {
        .uri = "/api/v1/config/mqtt",
        .method = HTTP_POST,
        .handler = update_mqtt_config_handler,
        .user_ctx = NULL
    },
    {
        .uri = "/*",
        .method = HTTP_GET,
        .handler = static_file_handler,
        .user_ctx = NULL
    }
};

/* Existing Handler Implementations */
static esp_err_t system_info_handler(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    system_diag_data_t diag_data;
    esp_err_t ret = system_manager_get_diagnostics(&diag_data);
    if (ret == ESP_OK) {
        cJSON_AddNumberToObject(root, "free_heap", diag_data.free_heap);
        cJSON_AddNumberToObject(root, "min_free_heap", diag_data.min_free_heap);
        cJSON_AddNumberToObject(root, "uptime_ms", diag_data.uptime_seconds * 1000);
        cJSON_AddNumberToObject(root, "cpu_usage", diag_data.cpu_usage);
        cJSON_AddNumberToObject(root, "internal_temp", diag_data.internal_temp);
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

    cJSON_AddStringToObject(root, "status", 
        network_manager_is_connected() ? "Connected" : "Disconnected");
    
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

/* Configuration GET Handlers */
static esp_err_t get_network_config_handler(httpd_req_t *req)
{
    network_config_t config;
    esp_err_t ret = system_manager_load_network_config(&config);
    if (ret != ESP_OK) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    cJSON *root = cJSON_CreateObject();
    if (!root) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    // Only expose SSID
    cJSON_AddStringToObject(root, "wifi_ssid", config.wifi_ssid);

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

static esp_err_t get_mqtt_config_handler(httpd_req_t *req)
{
    mqtt_config_t config;
    esp_err_t ret = system_manager_load_mqtt_config(&config);
    if (ret != ESP_OK) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    cJSON *root = cJSON_CreateObject();
    if (!root) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    // Only expose broker URL
    cJSON_AddStringToObject(root, "broker_url", config.broker_url);

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

/* Configuration POST Handlers */
static esp_err_t update_network_config_handler(httpd_req_t *req)
{
    char *content = malloc(req->content_len + 1);
    if (!content) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    int ret = httpd_req_recv(req, content, req->content_len);
    if (ret <= 0) {
        free(content);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    content[req->content_len] = '\0';

    cJSON *root = cJSON_Parse(content);
    free(content);

    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    network_config_t config;
    // Load existing config first
    esp_err_t err = system_manager_load_network_config(&config);
    if (err != ESP_OK) {
        cJSON_Delete(root);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    // Update only SSID and password
    cJSON *item;
    if ((item = cJSON_GetObjectItem(root, "wifi_ssid")) && item->valuestring) {
        strlcpy(config.wifi_ssid, item->valuestring, sizeof(config.wifi_ssid));
    }
    if ((item = cJSON_GetObjectItem(root, "wifi_password")) && item->valuestring) {
        strlcpy(config.wifi_password, item->valuestring, sizeof(config.wifi_password));
    }

    cJSON_Delete(root);

    err = system_manager_save_network_config(&config);
    if (err != ESP_OK) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    // Send response before potential WiFi reconnect
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");

    // Update network manager after sending response
    network_manager_update_config();

    return ESP_OK;
}

static esp_err_t update_mqtt_config_handler(httpd_req_t *req)
{
    char *content = malloc(req->content_len + 1);
    if (!content) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    int ret = httpd_req_recv(req, content, req->content_len);
    if (ret <= 0) {
        free(content);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    content[req->content_len] = '\0';

    cJSON *root = cJSON_Parse(content);
    free(content);

    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    mqtt_config_t config;
    esp_err_t err = system_manager_load_mqtt_config(&config);
    if (err != ESP_OK) {
        cJSON_Delete(root);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    // Update only broker URL
    cJSON *item;
    if ((item = cJSON_GetObjectItem(root, "broker_url")) && item->valuestring) {
        strlcpy(config.broker_url, item->valuestring, sizeof(config.broker_url));
    }

    cJSON_Delete(root);

    err = system_manager_save_mqtt_config(&config);
    if (err != ESP_OK) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");

    return ESP_OK;
}

/* Static File Handler Implementation */
static esp_err_t static_file_handler(httpd_req_t *req)
{
    char filepath[FILE_PATH_MAX];
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
        ERROR_LOG_ERROR(TAG, ESP_ERR_NO_MEM, ERROR_CAT_SYSTEM,
            "Filepath buffer too small");
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Trying to serve file: %s", filepath);

    if (stat(filepath, &file_stat) == -1) {
        ERROR_LOG_ERROR(TAG, ESP_FAIL, ERROR_CAT_STORAGE,
            "Failed to stat file: %s", filepath);
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    fd = fopen(filepath, "r");
    if (!fd) {
        ERROR_LOG_ERROR(TAG, ESP_FAIL, ERROR_CAT_STORAGE,
            "Failed to open file: %s", filepath);
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
        ERROR_LOG_ERROR(TAG, ESP_ERR_NO_MEM, ERROR_CAT_SYSTEM,
            "Memory allocation failed");
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
                ERROR_LOG_ERROR(TAG, ESP_FAIL, ERROR_CAT_COMMUNICATION, 
                    "File sending failed!");
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

/* Server Initialization and Cleanup */
esp_err_t http_server_init(const http_server_config_t *config)
{
    if (server) {
        ERROR_LOG_WARNING(TAG, ESP_ERR_INVALID_STATE, ERROR_CAT_SYSTEM,
            "HTTP server already running");
        return ESP_ERR_INVALID_STATE;
    }

    // Initialize SPIFFS first
    esp_err_t ret = init_spiffs();
    if (ret != ESP_OK) {
        return ret;
    }

    httpd_config_t http_config = HTTPD_DEFAULT_CONFIG();
    http_config.server_port = config->port;
    http_config.max_open_sockets = config->max_clients;
    http_config.max_uri_handlers = 10;     // Add this line to increase from default
    http_config.lru_purge_enable = true;
    http_config.uri_match_fn = httpd_uri_match_wildcard;
    http_config.core_id = 0;
    http_config.task_priority = 1;
    http_config.stack_size = 8192;

    ESP_LOGI(TAG, "Starting HTTP server on port: %d", config->port);
    ret = httpd_start(&server, &http_config);
    if (ret != ESP_OK) {
        ERROR_LOG_ERROR(TAG, ret, ERROR_CAT_COMMUNICATION,
            "Failed to start HTTP server");
        return ret;
    }

    // Register URI handlers
    for (size_t i = 0; i < sizeof(uri_handlers) / sizeof(uri_handlers[0]); i++) {
        ESP_LOGI(TAG, "Registering URI handler: %s", uri_handlers[i].uri);
        if (httpd_register_uri_handler(server, &uri_handlers[i]) != ESP_OK) {
            ERROR_LOG_ERROR(TAG, ESP_FAIL, ERROR_CAT_SYSTEM,
                "Failed to register %s handler", uri_handlers[i].uri);
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

static esp_err_t sensor_data_handler(httpd_req_t *req) {
    dht11_reading_t reading;
    esp_err_t ret = data_manager_get_latest_data("dht11", &reading);
    
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    if (ret == ESP_OK && reading.valid) {
        cJSON_AddNumberToObject(root, "temperature", reading.temperature);
        cJSON_AddNumberToObject(root, "humidity", reading.humidity);
        cJSON_AddNumberToObject(root, "timestamp", reading.timestamp);
        cJSON_AddBoolToObject(root, "valid", true);
    } else {
        cJSON_AddBoolToObject(root, "valid", false);
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

http_server_config_t http_server_get_default_config(void) {
    http_server_config_t config = {
        .port = 80,
        .max_clients = 4,
        .enable_cors = true
    };
    return config;
}

esp_err_t http_server_init_default(void) {
    http_server_config_t config = http_server_get_default_config();
    return http_server_init(&config);
}
