#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    bool initialized;
    bool started;
    bool connected;
    bool boot_event_sent;
    uint32_t publish_count;
    char broker_uri[128];
    char client_id[64];
    char base_topic[128];
} mqtt_service_status_t;

esp_err_t mqtt_service_init(void);

bool mqtt_service_is_connected(void);
const mqtt_service_status_t *mqtt_service_get_status(void);

esp_err_t mqtt_service_publish_health(void);
esp_err_t mqtt_service_publish_event(const char *event_name, float confidence);

#ifdef __cplusplus
}
#endif