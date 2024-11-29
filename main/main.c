#include <stdio.h>
#include "led_strip_encoder.h"
#include "driver/rmt_tx.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "math.h"

static const char *TAG = "rainbow";

#define RMT_LED_STRIP_RESOLUTION_HZ  10000000  // 10MHz resolution
#define LED_STRIP_GPIO    38
#define NUM_PIXELS        1

// Function to convert HSV to RGB
void hsv2rgb(float h, float s, float v, uint32_t *r, uint32_t *g, uint32_t *b)
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

void app_main(void)
{
    ESP_LOGI(TAG, "Create RMT TX channel");
    rmt_channel_handle_t led_chan = NULL;
    rmt_tx_channel_config_t tx_chan_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .gpio_num = LED_STRIP_GPIO,
        .mem_block_symbols = 64,
        .resolution_hz = RMT_LED_STRIP_RESOLUTION_HZ,
        .trans_queue_depth = 4,
    };
    ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_chan_config, &led_chan));

    ESP_LOGI(TAG, "Install led strip encoder");
    rmt_encoder_handle_t led_encoder = NULL;
    led_strip_encoder_config_t encoder_config = {
        .resolution = RMT_LED_STRIP_RESOLUTION_HZ,
    };
    ESP_ERROR_CHECK(rmt_new_led_strip_encoder(&encoder_config, &led_encoder));

    ESP_LOGI(TAG, "Enable RMT TX channel");
    ESP_ERROR_CHECK(rmt_enable(led_chan));

    ESP_LOGI(TAG, "Start rainbow effect");
    
    rmt_transmit_config_t tx_config = {
        .loop_count = 0,
    };

    uint32_t red = 0;
    uint32_t green = 0;
    uint32_t blue = 0;
    float hue = 0;
    float saturation = 1.0;    // Full saturation
    float value = 0.01;        // 1% brightness

    // LED colors buffer
    uint8_t led_strip_pixels[3 * NUM_PIXELS];

    while (1) {
        hsv2rgb(hue, saturation, value, &red, &green, &blue);

        // Fill LED buffer
        led_strip_pixels[0] = green;  // Note: GRB order
        led_strip_pixels[1] = red;
        led_strip_pixels[2] = blue;

        // Transmit RGB data
        ESP_ERROR_CHECK(rmt_transmit(led_chan, led_encoder, led_strip_pixels, 3 * NUM_PIXELS, &tx_config));
        
        // Wait for transmission done
        ESP_ERROR_CHECK(rmt_tx_wait_all_done(led_chan, portMAX_DELAY));

        // Update hue for rainbow effect
        hue += 1;
        if (hue >= 360) {
            hue = 0;
        }

        vTaskDelay(pdMS_TO_TICKS(50));  // Control rainbow speed
    }
}
