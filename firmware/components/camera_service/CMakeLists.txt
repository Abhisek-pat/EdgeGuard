idf_component_register(
    SRCS "camera_service.c"
    INCLUDE_DIRS "include"
    REQUIRES log esp_timer esp32-camera esp_psram
)