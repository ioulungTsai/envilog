idf_component_register(
    SRCS "system_manager.c"
    INCLUDE_DIRS "include"
    REQUIRES nvs_flash esp_common envilog_config esp_timer esp_driver_tsens network_manager task_manager spiffs dht11_sensor envilog_mqtt
)
