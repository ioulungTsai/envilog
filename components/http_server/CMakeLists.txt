idf_component_register(
    SRCS 
        "http_server.c"
    INCLUDE_DIRS 
        "include"
    REQUIRES 
        "esp_http_server"
        "esp_wifi"
        "esp_timer"
        "json"
        "network_manager"
        "envilog_config"
        "vfs"
        "dht11_sensor"
        "error_handler"

)
