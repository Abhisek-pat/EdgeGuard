idf_component_register(
    SRCS "network_manager.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "../../main"
    REQUIRES log esp_event esp_netif esp_wifi nvs_flash
)