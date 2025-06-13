#pragma once

#include "esp_err.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief LED status modes
 */
typedef enum {
    LED_STATUS_OFF,           // LED off
    LED_STATUS_BOOT,          // Dim white during boot/power-up
    LED_STATUS_AP_MODE,       // Orange breathing (AP mode)
    LED_STATUS_STATION_MODE,  // Blue breathing (Station mode)
    LED_STATUS_ERROR          // Red steady for errors
} led_status_t;

/**
 * @brief Initialize the builtin LED
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t builtin_led_init(void);

/**
 * @brief Set LED status mode
 * 
 * @param status LED status to set
 * @return esp_err_t ESP_OK on success
 */
esp_err_t builtin_led_set_status(led_status_t status);

/**
 * @brief Deinitialize the builtin LED
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t builtin_led_deinit(void);

#ifdef __cplusplus
}
#endif
