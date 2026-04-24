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
    int input_width;
    int input_height;
    int input_channels;
    float input_scale;
    int input_zero_point;
    float output_scale;
    int output_zero_point;
    size_t tensor_arena_size;
} model_runner_info_t;

esp_err_t model_runner_init(void);
esp_err_t model_runner_get_info(model_runner_info_t *info);

/*
 * input_u8 must be a 64x64 grayscale image flattened to 4096 bytes.
 * Output is a dequantized person score in [approximately] 0..1.
 */
esp_err_t model_runner_infer_u8(const uint8_t *input_u8, size_t input_len, float *person_score);

#ifdef __cplusplus
}
#endif