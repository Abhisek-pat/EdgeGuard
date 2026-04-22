#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_camera.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    bool ready;
    pixformat_t pixel_format;
    framesize_t frame_size;
    uint8_t fb_count;
    size_t last_frame_len;
    uint16_t last_width;
    uint16_t last_height;
    int64_t last_capture_time_us;
    uint32_t capture_count;
} camera_service_status_t;

esp_err_t camera_service_init(void);
esp_err_t camera_service_capture_test(void);

camera_fb_t *camera_service_get_frame(void);
void camera_service_return_frame(camera_fb_t *fb);

bool camera_service_is_ready(void);
const camera_service_status_t *camera_service_get_status(void);

#ifdef __cplusplus
}
#endif