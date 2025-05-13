#pragma once

#include "esp_err.h"
#include "esp_http_server.h"

#ifdef __cplusplus
extern "C" {
#endif

// File handling constants
#define FILE_PATH_MAX (ESP_VFS_PATH_MAX + CONFIG_SPIFFS_OBJ_NAME_LEN)
#define HTTP_CHUNK_SIZE (1024)

/**
 * @brief HTTP server configuration
 */
typedef struct {
    uint16_t port;              /*!< Server port number */
    size_t max_clients;         /*!< Maximum number of clients */
    bool enable_cors;           /*!< Enable CORS support */
} http_server_config_t;

/**
 * @brief Initialize and start HTTP server
 * 
 * @param config Server configuration parameters
 * @return esp_err_t ESP_OK on success
 */
esp_err_t http_server_init(const http_server_config_t *config);

/**
 * @brief Stop and cleanup HTTP server
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t http_server_stop(void);

/**
 * @brief Get default HTTP server configuration
 * 
 * @return http_server_config_t Default configuration
 */
http_server_config_t http_server_get_default_config(void);

/**
 * @brief Initialize and start HTTP server with default configuration
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t http_server_init_default(void);

#ifdef __cplusplus
}
#endif
