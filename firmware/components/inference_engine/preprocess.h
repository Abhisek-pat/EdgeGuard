#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_camera.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t preprocess_resize_grayscale(
    const camera_fb_t *fb,
    uint8_t *out,
    size_t out_len,
    int out_w,
    int out_h
);

#ifdef __cplusplus
}
#endif