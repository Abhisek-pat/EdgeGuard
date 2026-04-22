idf_component_register(
    SRCS "inference_engine.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "../../main"
    REQUIRES log freertos esp_timer camera_service event_manager
)