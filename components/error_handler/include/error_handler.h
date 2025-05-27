#pragma once

#include "esp_err.h"
#include "esp_log.h"
#include <string.h>

// Extract filename from full path
// Use ESP-IDF's built-in __FILENAME__ or create our own with different name
#ifndef __FILENAME__
#define ERROR_FILENAME (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#else
#define ERROR_FILENAME __FILENAME__
#endif

/**
 * @brief Error categories for classification
 */
typedef enum {
    ERROR_CAT_HARDWARE = 0,     // GPIO, sensors, hardware interfaces
    ERROR_CAT_NETWORK,          // WiFi, connectivity
    ERROR_CAT_STORAGE,          // NVS, SPIFFS, file operations
    ERROR_CAT_SENSOR,           // Sensor readings, validation
    ERROR_CAT_SYSTEM,           // Memory, tasks, system resources
    ERROR_CAT_COMMUNICATION,    // HTTP, MQTT, protocols
    ERROR_CAT_CONFIG,           // Configuration loading/saving
    ERROR_CAT_VALIDATION        // Data validation, parsing
} error_category_t;

/**
 * @brief Get category name string
 */
const char* error_category_name(error_category_t category);

/**
 * @brief Standardized error logging (replaces ESP_LOGE)
 */
#define ERROR_LOG_ERROR(tag, code, category, fmt, ...) \
    ESP_LOGE(tag, "[%s:%s:%d][%s][%s] " fmt " (0x%x)", \
             __FILENAME__, __FUNCTION__, __LINE__, \
             esp_err_to_name(code), \
             error_category_name(category), \
             ##__VA_ARGS__, code)

/**
 * @brief Standardized warning logging (replaces ESP_LOGW)
 */
#define ERROR_LOG_WARNING(tag, code, category, fmt, ...) \
    ESP_LOGW(tag, "[%s:%s:%d][%s][%s] " fmt " (0x%x)", \
             __FILENAME__, __FUNCTION__, __LINE__, \
             esp_err_to_name(code), \
             error_category_name(category), \
             ##__VA_ARGS__, code)
