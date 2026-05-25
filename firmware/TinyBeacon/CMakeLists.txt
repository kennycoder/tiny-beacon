idf_component_register(
    SRCS "TinyBeacon.cpp" "TinyBeacon_Arduino.cpp" "TinyBeacon_IDF.cpp"
    INCLUDE_DIRS "."
    REQUIRES nvs_flash bt esp_wifi esp_event esp_netif
)
