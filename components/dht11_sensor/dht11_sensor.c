#include <string.h>
#include "dht11_sensor.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "rom/ets_sys.h"
#include "envilog_mqtt.h"
#include "cJSON.h"

static const char *TAG = "dht11";
static uint8_t dht_gpio;
static TaskHandle_t dht11_task_handle = NULL;
static dht11_reading_t last_reading = {0};
static bool sensor_running = false;
static int64_t last_read_time = -2000000;

// Helper function for timing-based reading
static int wait_or_timeout(uint16_t microSeconds, int level) {
    int micros_ticks = 0;
    while(gpio_get_level(dht_gpio) == level) { 
        if(micros_ticks++ > microSeconds) {
            ESP_LOGW(TAG, "Timeout waiting for level %d after %d microseconds", level, micros_ticks);
            return ESP_ERR_TIMEOUT;
        }
        ets_delay_us(1);
    }
    return micros_ticks;
}

static esp_err_t read_raw_data(uint8_t data[5]) {
    memset(data, 0, 5);
    
    // Start signal
    gpio_set_direction(dht_gpio, GPIO_MODE_OUTPUT);
    gpio_set_level(dht_gpio, 0);
    ets_delay_us(20000);    // 20ms low
    gpio_set_level(dht_gpio, 1);
    ets_delay_us(40);       // 40us high
    gpio_set_direction(dht_gpio, GPIO_MODE_INPUT);
    
    ESP_LOGD(TAG, "Waiting for DHT11 response");
    
    // Check response (low 80us, high 80us)
    if(wait_or_timeout(80, 0) == ESP_ERR_TIMEOUT) return ESP_ERR_TIMEOUT;
    if(wait_or_timeout(80, 1) == ESP_ERR_TIMEOUT) return ESP_ERR_TIMEOUT;
    
    // Read 40 bits (5 bytes)
    for(int i = 0; i < 40; i++) {
        if(wait_or_timeout(50, 0) == ESP_ERR_TIMEOUT) return ESP_ERR_TIMEOUT;
        
        int high_duration = wait_or_timeout(70, 1);
        if(high_duration == ESP_ERR_TIMEOUT) return ESP_ERR_TIMEOUT;
        
        if(high_duration > 28) {
            data[i/8] |= (1 << (7-(i%8)));
        }
    }
    
    ESP_LOGD(TAG, "Read complete: %02x %02x %02x %02x %02x", 
             data[0], data[1], data[2], data[3], data[4]);
             
    // Verify checksum
    if(data[4] != ((data[0] + data[1] + data[2] + data[3]) & 0xFF)) {
        ESP_LOGW(TAG, "Checksum failed");
        return ESP_ERR_INVALID_CRC;
    }
    
    return ESP_OK;
}

static esp_err_t publish_reading(const dht11_reading_t *reading) {
    if (!reading->valid) {
        return ESP_FAIL;
    }

    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        return ESP_ERR_NO_MEM;
    }

    cJSON_AddNumberToObject(root, "temperature", reading->temperature);
    cJSON_AddNumberToObject(root, "humidity", reading->humidity);
    cJSON_AddNumberToObject(root, "timestamp", reading->timestamp);

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (json_str == NULL) {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t ret = envilog_mqtt_publish_diagnostic("dht11", json_str, strlen(json_str));
    free(json_str);

    return ret;
}

esp_err_t dht11_init(uint8_t gpio_num) {
    dht_gpio = gpio_num;
    
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << dht_gpio),
        .mode = GPIO_MODE_INPUT_OUTPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    
    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure GPIO");
        return ret;
    }

    // Set initial state
    gpio_set_level(dht_gpio, 1);
    
    ESP_LOGI(TAG, "DHT11 initialized on GPIO%d", dht_gpio);
    return ESP_OK;
}

esp_err_t dht11_read(dht11_reading_t *reading) {
    if (reading == NULL) return ESP_ERR_INVALID_ARG;
    
    // Check if 2 seconds have passed since last read
    if(esp_timer_get_time() - 2000000 < last_read_time) {
        memcpy(reading, &last_reading, sizeof(dht11_reading_t));
        return ESP_OK;
    }
    
    uint8_t data[5] = {0};
    esp_err_t ret = read_raw_data(data);
    
    if (ret == ESP_OK) {
        reading->humidity = (float)data[0] + (float)data[1]/10.0f;
        reading->temperature = (float)data[2] + (float)data[3]/10.0f;
        reading->timestamp = esp_timer_get_time() / 1000;  // microseconds to milliseconds
        reading->valid = true;
        
        // Update last reading
        memcpy(&last_reading, reading, sizeof(dht11_reading_t));
        last_read_time = esp_timer_get_time();
        
        ESP_LOGI(TAG, "Temperature: %.1f°C, Humidity: %.1f%%", 
                 reading->temperature, reading->humidity);
    } else {
        reading->valid = false;
    }
    
    return ret;
}

static void dht11_reading_task(void *pvParameters) {
    uint32_t read_interval_ms = (uint32_t)pvParameters;
    TickType_t last_wake_time = xTaskGetTickCount();

    while (1) {
        // Read sensor
        dht11_reading_t reading;
        esp_err_t ret = dht11_read(&reading);

        if (ret == ESP_OK && reading.valid) {
            // Publish reading if connected
            if (envilog_mqtt_is_connected()) {
                publish_reading(&reading);
            }
        } else {
            ESP_LOGW(TAG, "Failed to read DHT11: %s", esp_err_to_name(ret));
            // Add delay between retries on error
            vTaskDelay(pdMS_TO_TICKS(2000));
        }
        
        // Wait for next reading interval
        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(read_interval_ms));
    }
}

esp_err_t dht11_start_reading(uint32_t read_interval_ms) {
    if (sensor_running) {
        return ESP_ERR_INVALID_STATE;
    }

    BaseType_t ret = xTaskCreate(
        dht11_reading_task,
        "dht11_task",
        4096,  // Stack size
        (void*)read_interval_ms,
        5,     // Priority
        &dht11_task_handle
    );

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create DHT11 task");
        return ESP_FAIL;
    }

    sensor_running = true;
    ESP_LOGI(TAG, "DHT11 reading task started with interval %lu ms", read_interval_ms);
    return ESP_OK;
}

esp_err_t dht11_stop_reading(void) {
    if (!sensor_running || dht11_task_handle == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    vTaskDelete(dht11_task_handle);
    dht11_task_handle = NULL;
    sensor_running = false;
    
    ESP_LOGI(TAG, "DHT11 reading task stopped");
    return ESP_OK;
}

esp_err_t dht11_get_last_reading(dht11_reading_t *reading) {
    if (reading == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(reading, &last_reading, sizeof(dht11_reading_t));
    return ESP_OK;
}

esp_err_t dht11_publish_diagnostics(void) {
    dht11_reading_t reading;
    if (dht11_get_last_reading(&reading) == ESP_OK && reading.valid) {
        // Create JSON string for sensor data
        cJSON *root = cJSON_CreateObject();
        if (root) {
            cJSON_AddNumberToObject(root, "temperature", reading.temperature);
            cJSON_AddNumberToObject(root, "humidity", reading.humidity);
            cJSON_AddNumberToObject(root, "timestamp", reading.timestamp);
            
            char *json_str = cJSON_PrintUnformatted(root);
            if (json_str) {
                // Publish to MQTT if connected
                if (envilog_mqtt_is_connected()) {
                    envilog_mqtt_publish_diagnostic("sensors/dht11", json_str, strlen(json_str));
                }
                free(json_str);
            }
            cJSON_Delete(root);
        }
    }
    
    return ESP_OK;
}
