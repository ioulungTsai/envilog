idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "include"
    REQUIRES freertos esp_system esp_timer esp_wifi nvs_flash 
)