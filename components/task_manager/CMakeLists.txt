idf_component_register(
    SRCS "task_manager.c" "system_monitor_msg.c"
    INCLUDE_DIRS "include"
    REQUIRES "freertos"
             "esp_timer"
             "esp_system"
             "esp_event"
             "nvs_flash"
             "esp_wifi"
)
