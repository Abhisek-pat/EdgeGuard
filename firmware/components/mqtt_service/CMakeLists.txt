idf_component_register(
    SRCS "mqtt_service.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "../../main"
    REQUIRES log freertos mqtt network_manager camera_service
)