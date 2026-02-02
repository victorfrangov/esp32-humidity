idf_component_register(
    SRCS "main.c" "wifi.c" "weather.c" "ble.c" "dht20.c" "geolocation.c"
    INCLUDE_DIRS "."
    REQUIRES driver esp_http_client cjson esp_wifi nvs_flash u8g2 u8g2-hal-esp-idf bt
)