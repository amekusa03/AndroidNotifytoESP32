idf_component_register(
    SRCS "main.cpp"
    INCLUDE_DIRS "."
    REQUIRES
        LovyanGFX
        bt
        nvs_flash
        driver
        esp_wifi
        esp_event
        esp_netif
        lwip
)

