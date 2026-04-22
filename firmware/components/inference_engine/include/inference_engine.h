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
    bool running;
    bool warmup_complete;
    bool trigger_latched;

    uint32_t sample_count;
    uint32_t trigger_count;
    uint32_t dropped_frame_count;

    size_t baseline_frame_len;
    size_t last_frame_len;

    float last_score;
    float trigger_threshold;

    int64_t last_inference_time_us;
    char last_decision[24];
} inference_engine_status_t;

esp_err_t inference_engine_init(void);
const inference_engine_status_t *inference_engine_get_status(void);

#ifdef __cplusplus
}
#endif