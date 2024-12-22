#pragma once

#include "esp_err.h"
#include "esp_http_server.h"
#include "esp_timer.h"
#include "esp_chip_info.h"
#include "esp_cpu.h"
#include "network_manager.h"

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
