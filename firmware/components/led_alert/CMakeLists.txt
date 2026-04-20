idf_component_register(
    SRCS "led_alert.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "../../main"
    REQUIRES log driver freertos
)