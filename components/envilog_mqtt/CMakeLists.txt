idf_component_register(
    SRCS "envilog_mqtt.c"
    INCLUDE_DIRS "include"
    REQUIRES "mqtt"
             "esp_wifi"
             "network_manager"
             "task_manager"
             "envilog_config"
             "system_manager"
             "json"
             "error_handler"
             "data_manager"
)
