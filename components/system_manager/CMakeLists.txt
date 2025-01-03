idf_component_register(
    SRCS "system_manager.c"
    INCLUDE_DIRS "include"
    REQUIRES nvs_flash esp_common envilog_config
)
