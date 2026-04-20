#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

typedef struct
{
    bool wifi_connected;
    bool mqtt_connected;
    bool alert_active;

    uint32_t boot_count;
    uint32_t event_count;
    uint32_t uptime_sec;
    uint32_t last_event_ms;

    float threshold;
    float last_confidence;

    SemaphoreHandle_t mutex;
} edgeguard_state_t;

void app_state_init(void);
edgeguard_state_t *app_state_get(void);

void app_state_set_wifi_connected(bool value);
void app_state_set_mqtt_connected(bool value);
void app_state_set_alert_active(bool value);

void app_state_set_threshold(float value);
void app_state_set_last_confidence(float value);
void app_state_increment_event_count(void);
void app_state_set_last_event_ms(uint32_t value);
void app_state_set_uptime_sec(uint32_t value);
