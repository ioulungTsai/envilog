idf_component_register(
    SRCS "data_manager.c"
    INCLUDE_DIRS "include"
    REQUIRES "esp_common"
             "freertos"
             "dht11_sensor"
)
