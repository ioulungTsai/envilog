#include <string.h>
#include "builtin_led.h"
#include "driver/rmt_tx.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "math.h"
#include "error_handler.h"

static const char *TAG = "builtin_led";

/**
 * @brief Type of led strip encoder configuration
 */
typedef struct {
    uint32_t resolution; /*!< Encoder resolution, in Hz */
} led_strip_encoder_config_t;

// Hardware configuration
#define RMT_LED_STRIP_RESOLUTION_HZ  10000000  // 10MHz resolution
#define LED_STRIP_GPIO               38        // ESP32-S3 DevKit GPIO
#define NUM_PIXELS                   1

// Animation configuration - GAMMA CORRECTED VERSION
#define BREATHING_CYCLE_MS           4000      // 4 seconds per breathing cycle
#define ANIMATION_UPDATE_MS          20        // 20ms updates (50fps)

// Blue brightness
#define MAX_BRIGHTNESS_BLUE          0.003f    // 0.3% maximum
#define MIN_BRIGHTNESS_BLUE          0.0001f   // 0.01% minimum

// Green brightness
#define MAX_BRIGHTNESS_GREEN         0.00011f  // 0.011% maximum
#define MIN_BRIGHTNESS_GREEN         0.000005f // 0.0005% minimum

// Error brightness (red steady light)
#define BRIGHTNESS_BOOT              0.004f    // 0.4% brightness for dim white
#define BRIGHTNESS_ERROR             0.003f    // 0.3% for red error

#define GAMMA_CORRECTION             0.45f     // Gamma value for better visibility

// Color definitions (HSV Hue values)
#define HUE_GREEN                   120.0f     // Green for AP mode
#define HUE_BLUE                    240.0f     // Blue for Station mode
#define HUE_RED                     0.0f       // Red for error
#define SATURATION                  1.0f       // Full saturation
#define HUE_WHITE                   0.0f       // White (any hue works with 0 saturation)
#define SATURATION_WHITE            0.0f       // No saturation = white

// Breathing animation helper
#define BREATHING_STEPS_PER_CYCLE   (BREATHING_CYCLE_MS / ANIMATION_UPDATE_MS)  // 150 steps per cycle

// RMT encoder from your example (included inline)
typedef struct {
    rmt_encoder_t base;
    rmt_encoder_t *bytes_encoder;
    rmt_encoder_t *copy_encoder;
    int state;
    rmt_symbol_word_t reset_code;
} rmt_led_strip_encoder_t;

// Global state
static rmt_channel_handle_t led_chan = NULL;
static rmt_encoder_handle_t led_encoder = NULL;
static TaskHandle_t led_task_handle = NULL;
static led_status_t current_status = LED_STATUS_OFF;
static bool led_initialized = false;

// Function prototypes
static void hsv2rgb(float h, float s, float v, uint32_t *r, uint32_t *g, uint32_t *b);
static void led_animation_task(void *pvParameters);
static esp_err_t setup_rmt_encoder(void);
static esp_err_t transmit_led_color(uint32_t red, uint32_t green, uint32_t blue);

// RMT encoder implementation (from your example)
static size_t rmt_encode_led_strip(rmt_encoder_t *encoder, rmt_channel_handle_t channel, 
                                  const void *primary_data, size_t data_size, 
                                  rmt_encode_state_t *ret_state)
{
    rmt_led_strip_encoder_t *led_encoder = __containerof(encoder, rmt_led_strip_encoder_t, base);
    rmt_encoder_handle_t bytes_encoder = led_encoder->bytes_encoder;
    rmt_encoder_handle_t copy_encoder = led_encoder->copy_encoder;
    rmt_encode_state_t session_state = RMT_ENCODING_RESET;
    rmt_encode_state_t state = RMT_ENCODING_RESET;
    size_t encoded_symbols = 0;
    
    switch (led_encoder->state) {
    case 0: // send RGB data
        encoded_symbols += bytes_encoder->encode(bytes_encoder, channel, primary_data, data_size, &session_state);
        if (session_state & RMT_ENCODING_COMPLETE) {
            led_encoder->state = 1;
        }
        if (session_state & RMT_ENCODING_MEM_FULL) {
            state |= RMT_ENCODING_MEM_FULL;
            goto out;
        }
    // fall-through
    case 1: // send reset code
        encoded_symbols += copy_encoder->encode(copy_encoder, channel, &led_encoder->reset_code,
                                                sizeof(led_encoder->reset_code), &session_state);
        if (session_state & RMT_ENCODING_COMPLETE) {
            led_encoder->state = RMT_ENCODING_RESET;
            state |= RMT_ENCODING_COMPLETE;
        }
        if (session_state & RMT_ENCODING_MEM_FULL) {
            state |= RMT_ENCODING_MEM_FULL;
            goto out;
        }
    }
out:
    *ret_state = state;
    return encoded_symbols;
}

static esp_err_t rmt_del_led_strip_encoder(rmt_encoder_t *encoder)
{
    rmt_led_strip_encoder_t *led_encoder = __containerof(encoder, rmt_led_strip_encoder_t, base);
    rmt_del_encoder(led_encoder->bytes_encoder);
    rmt_del_encoder(led_encoder->copy_encoder);
    free(led_encoder);
    return ESP_OK;
}

static esp_err_t rmt_led_strip_encoder_reset(rmt_encoder_t *encoder)
{
    rmt_led_strip_encoder_t *led_encoder = __containerof(encoder, rmt_led_strip_encoder_t, base);
    rmt_encoder_reset(led_encoder->bytes_encoder);
    rmt_encoder_reset(led_encoder->copy_encoder);
    led_encoder->state = RMT_ENCODING_RESET;
    return ESP_OK;
}

static esp_err_t rmt_new_led_strip_encoder(const led_strip_encoder_config_t *config, rmt_encoder_handle_t *ret_encoder)
{
    esp_err_t ret = ESP_OK;
    rmt_led_strip_encoder_t *led_encoder = NULL;
    
    if (!config || !ret_encoder) {
        return ESP_ERR_INVALID_ARG;
    }
    
    led_encoder = calloc(1, sizeof(rmt_led_strip_encoder_t));
    if (!led_encoder) {
        return ESP_ERR_NO_MEM;
    }
    
    led_encoder->base.encode = rmt_encode_led_strip;
    led_encoder->base.del = rmt_del_led_strip_encoder;
    led_encoder->base.reset = rmt_led_strip_encoder_reset;
    
    // WS2812 timing configuration
    rmt_bytes_encoder_config_t bytes_encoder_config = {
        .bit0 = {
            .level0 = 1,
            .duration0 = 0.3 * config->resolution / 1000000, // T0H=0.3us
            .level1 = 0,
            .duration1 = 0.9 * config->resolution / 1000000, // T0L=0.9us
        },
        .bit1 = {
            .level0 = 1,
            .duration0 = 0.9 * config->resolution / 1000000, // T1H=0.9us
            .level1 = 0,
            .duration1 = 0.3 * config->resolution / 1000000, // T1L=0.3us
        },
        .flags.msb_first = 1 // WS2812 transfer bit order: G7...G0R7...R0B7...B0
    };
    
    ret = rmt_new_bytes_encoder(&bytes_encoder_config, &led_encoder->bytes_encoder);
    if (ret != ESP_OK) {
        goto err;
    }
    
    rmt_copy_encoder_config_t copy_encoder_config = {};
    ret = rmt_new_copy_encoder(&copy_encoder_config, &led_encoder->copy_encoder);
    if (ret != ESP_OK) {
        goto err;
    }

    uint32_t reset_ticks = config->resolution / 1000000 * 50 / 2;
    led_encoder->reset_code = (rmt_symbol_word_t) {
        .level0 = 0,
        .duration0 = reset_ticks,
        .level1 = 0,
        .duration1 = reset_ticks,
    };
    
    *ret_encoder = &led_encoder->base;
    return ESP_OK;
    
err:
    if (led_encoder) {
        if (led_encoder->bytes_encoder) {
            rmt_del_encoder(led_encoder->bytes_encoder);
        }
        if (led_encoder->copy_encoder) {
            rmt_del_encoder(led_encoder->copy_encoder);
        }
        free(led_encoder);
    }
    return ret;
}

// HSV to RGB conversion
static void hsv2rgb(float h, float s, float v, uint32_t *r, uint32_t *g, uint32_t *b)
{
    float c = v * s;
    float x = c * (1 - fabs(fmod(h / 60.0f, 2) - 1));
    float m = v - c;
    float rs, gs, bs;

    if (h >= 0 && h < 60) {
        rs = c; gs = x; bs = 0;
    } else if (h >= 60 && h < 120) {
        rs = x; gs = c; bs = 0;
    } else if (h >= 120 && h < 180) {
        rs = 0; gs = c; bs = x;
    } else if (h >= 180 && h < 240) {
        rs = 0; gs = x; bs = c;
    } else if (h >= 240 && h < 300) {
        rs = x; gs = 0; bs = c;
    } else {
        rs = c; gs = 0; bs = x;
    }

    *r = (rs + m) * 255;
    *g = (gs + m) * 255;
    *b = (bs + m) * 255;
}

// Setup RMT encoder
static esp_err_t setup_rmt_encoder(void)
{
    ESP_LOGI(TAG, "Create RMT TX channel");
    rmt_tx_channel_config_t tx_chan_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .gpio_num = LED_STRIP_GPIO,
        .mem_block_symbols = 64,
        .resolution_hz = RMT_LED_STRIP_RESOLUTION_HZ,
        .trans_queue_depth = 4,
    };
    
    esp_err_t ret = rmt_new_tx_channel(&tx_chan_config, &led_chan);
    if (ret != ESP_OK) {
        ERROR_LOG_ERROR(TAG, ret, ERROR_CAT_HARDWARE, "Failed to create RMT TX channel");
        return ret;
    }

    ESP_LOGI(TAG, "Install led strip encoder");
    led_strip_encoder_config_t encoder_config = {
        .resolution = RMT_LED_STRIP_RESOLUTION_HZ,
    };
    ret = rmt_new_led_strip_encoder(&encoder_config, &led_encoder);
    if (ret != ESP_OK) {
        ERROR_LOG_ERROR(TAG, ret, ERROR_CAT_HARDWARE, "Failed to create LED encoder");
        return ret;
    }

    ESP_LOGI(TAG, "Enable RMT TX channel");
    ret = rmt_enable(led_chan);
    if (ret != ESP_OK) {
        ERROR_LOG_ERROR(TAG, ret, ERROR_CAT_HARDWARE, "Failed to enable RMT channel");
        return ret;
    }

    return ESP_OK;
}

// Transmit LED color
static esp_err_t transmit_led_color(uint32_t red, uint32_t green, uint32_t blue)
{
    uint8_t led_strip_pixels[3] = {
        (uint8_t)green,  // GRB order for WS2812
        (uint8_t)red,
        (uint8_t)blue
    };

    rmt_transmit_config_t tx_config = {
        .loop_count = 0,
    };

    esp_err_t ret = rmt_transmit(led_chan, led_encoder, led_strip_pixels, sizeof(led_strip_pixels), &tx_config);
    if (ret != ESP_OK) {
        return ret;
    }

    return rmt_tx_wait_all_done(led_chan, pdMS_TO_TICKS(100));
}

// LED animation task
static void led_animation_task(void *pvParameters)
{
    uint32_t cycle_counter = 0;
    float brightness;
    uint32_t red, green, blue;
    float hue;

    ESP_LOGI(TAG, "LED animation task started");

    while (1) {
        switch (current_status) {
            case LED_STATUS_OFF:
                // Turn off LED
                transmit_led_color(0, 0, 0);
                vTaskDelay(pdMS_TO_TICKS(100)); // Check status every 100ms when off
                break;

            case LED_STATUS_BOOT:
                // Dim white steady light during boot
                hsv2rgb(HUE_WHITE, SATURATION_WHITE, BRIGHTNESS_BOOT, &red, &green, &blue);
                transmit_led_color(red, green, blue);
                vTaskDelay(pdMS_TO_TICKS(ANIMATION_UPDATE_MS));
                break;

            case LED_STATUS_AP_MODE:
                hue = HUE_GREEN;
                goto breathing_effect;

            case LED_STATUS_STATION_MODE:
                hue = HUE_BLUE;
                goto breathing_effect;

            case LED_STATUS_ERROR:
                // Steady red
                hsv2rgb(HUE_RED, SATURATION, BRIGHTNESS_ERROR, &red, &green, &blue);
                transmit_led_color(red, green, blue);
                vTaskDelay(pdMS_TO_TICKS(ANIMATION_UPDATE_MS));
                break;

            breathing_effect:
                // Select brightness range based on current color
                float max_bright, min_bright;
                if (current_status == LED_STATUS_AP_MODE) {
                    // Green - use dimmer range
                    max_bright = MAX_BRIGHTNESS_GREEN;
                    min_bright = MIN_BRIGHTNESS_GREEN;
                } else {
                    // Blue - use your perfect range
                    max_bright = MAX_BRIGHTNESS_BLUE;
                    min_bright = MIN_BRIGHTNESS_BLUE;
                }
                
                float phase = (float)(cycle_counter % BREATHING_CYCLE_MS) / BREATHING_CYCLE_MS;
                float breathing_intensity = 0.5f * (1.0f + sinf(2.0f * M_PI * phase));
                float linear_brightness = min_bright + (max_bright - min_bright) * breathing_intensity;
                brightness = powf(linear_brightness, GAMMA_CORRECTION);
                
                hsv2rgb(hue, SATURATION, brightness, &red, &green, &blue);
                transmit_led_color(red, green, blue);
                
                cycle_counter += ANIMATION_UPDATE_MS;
                vTaskDelay(pdMS_TO_TICKS(ANIMATION_UPDATE_MS));
                break;
        }
    }
}

// Public API implementation
esp_err_t builtin_led_init(void)
{
    if (led_initialized) {
        ERROR_LOG_WARNING(TAG, ESP_ERR_INVALID_STATE, ERROR_CAT_SYSTEM, "LED already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing builtin LED");

    esp_err_t ret = setup_rmt_encoder();
    if (ret != ESP_OK) {
        ERROR_LOG_ERROR(TAG, ret, ERROR_CAT_HARDWARE, "Failed to setup RMT encoder");
        return ret;
    }

    // Create LED animation task
    BaseType_t task_ret = xTaskCreate(
        led_animation_task,
        "led_animation",
        4096,
        NULL,
        3, // Priority
        &led_task_handle
    );

    if (task_ret != pdPASS) {
        ERROR_LOG_ERROR(TAG, ESP_FAIL, ERROR_CAT_SYSTEM, "Failed to create LED animation task");
        return ESP_FAIL;
    }

    led_initialized = true;
    current_status = LED_STATUS_OFF;
    
    ESP_LOGI(TAG, "Builtin LED initialized successfully");
    return ESP_OK;
}

esp_err_t builtin_led_set_status(led_status_t status)
{
    if (!led_initialized) {
        ERROR_LOG_ERROR(TAG, ESP_ERR_INVALID_STATE, ERROR_CAT_SYSTEM, "LED not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (status == current_status) {
        return ESP_OK; // No change needed
    }

    ESP_LOGI(TAG, "Setting LED status to %d", status);
    current_status = status;
    return ESP_OK;
}

esp_err_t builtin_led_deinit(void)
{
    if (!led_initialized) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Deinitializing builtin LED");

    // Delete animation task
    if (led_task_handle) {
        vTaskDelete(led_task_handle);
        led_task_handle = NULL;
    }

    // Turn off LED
    transmit_led_color(0, 0, 0);

    // Cleanup RMT resources
    if (led_encoder) {
        rmt_del_encoder(led_encoder);
        led_encoder = NULL;
    }

    if (led_chan) {
        rmt_disable(led_chan);
        rmt_del_channel(led_chan);
        led_chan = NULL;
    }

    led_initialized = false;
    current_status = LED_STATUS_OFF;

    ESP_LOGI(TAG, "Builtin LED deinitialized");
    return ESP_OK;
}
