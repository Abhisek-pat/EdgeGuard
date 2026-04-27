#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    bool initialized;
    uint32_t accepted_events;
    uint32_t ignored_events;
    uint64_t last_event_ts_ms;
    float last_event_confidence;
    char last_event_name[32];
} event_manager_status_t;

typedef struct
{
    uint64_t timestamp_ms;
    char name[32];
    float confidence;
    bool accepted;
    bool is_test;
} edgeguard_recent_event_t;

esp_err_t event_manager_init(void);
esp_err_t event_manager_submit_event(const char *event_name, float confidence, bool is_test);
esp_err_t event_manager_trigger_test_event(void);

const event_manager_status_t *event_manager_get_status(void);
size_t event_manager_get_recent_events(edgeguard_recent_event_t *out_events, size_t max_items);

#ifdef __cplusplus
}
#endif