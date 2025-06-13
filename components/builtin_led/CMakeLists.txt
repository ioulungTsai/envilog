idf_component_register(
    SRCS "builtin_led.c"
    INCLUDE_DIRS "include"
    REQUIRES "esp_driver_rmt" 
             "freertos"
             "esp_common"
             "esp_timer"
)