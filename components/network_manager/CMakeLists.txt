idf_component_register(
    SRCS "network_manager.c"
    INCLUDE_DIRS "include"
    REQUIRES "freertos"
             "esp_timer"
             "esp_wifi"
             "esp_event"
             "esp_netif"
             "nvs_flash"
             "task_manager"
             "envilog_config"
             "esp_system"
)