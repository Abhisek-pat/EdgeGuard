idf_component_register(
    SRCS "web_server.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "../../main"
    REQUIRES log freertos esp_http_server camera_service network_manager mqtt_service event_manager
)