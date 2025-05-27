# components/dht11_sensor/CMakeLists.txt
idf_component_register(
    SRCS "dht11_sensor.c"
    INCLUDE_DIRS "include"
    REQUIRES 
            "driver" 
            "esp_timer" 
            "esp_common" 
            "freertos" 
            "task_manager" 
            "envilog_mqtt" 
            "json" 
            "esp_hw_support"
            "error_handler"
)
