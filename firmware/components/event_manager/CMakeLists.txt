idf_component_register(
    SRCS "event_manager.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "../../main"
    REQUIRES log freertos esp_timer led_alert mqtt_service
)