#include "preprocess.h"

#include <stdint.h>

esp_err_t preprocess_resize_grayscale(
    const camera_fb_t *fb,
    uint8_t *out,
    size_t out_len,
    int out_w,
    int out_h
)
{
    if (fb == NULL || out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (fb->format != PIXFORMAT_GRAYSCALE) {
        return ESP_ERR_INVALID_STATE;
    }

    if (out_w <= 0 || out_h <= 0) {
        return ESP_ERR_INVALID_ARG;
    }

    const size_t required = (size_t)(out_w * out_h);
    if (out_len < required) {
        return ESP_ERR_INVALID_SIZE;
    }

    const int in_w = fb->width;
    const int in_h = fb->height;
    const uint8_t *src = fb->buf;

    if (src == NULL || in_w <= 0 || in_h <= 0) {
        return ESP_ERR_INVALID_STATE;
    }

    /*
     * Box-average resize from input grayscale frame to model input.
     * This is heavier than nearest-neighbor, but it better matches
     * offline resize behavior and gives cleaner model inputs.
     */
    for (int oy = 0; oy < out_h; ++oy) {
        int y0 = (oy * in_h) / out_h;
        int y1 = ((oy + 1) * in_h) / out_h;
        if (y1 <= y0) {
            y1 = y0 + 1;
        }
        if (y1 > in_h) {
            y1 = in_h;
        }

        for (int ox = 0; ox < out_w; ++ox) {
            int x0 = (ox * in_w) / out_w;
            int x1 = ((ox + 1) * in_w) / out_w;
            if (x1 <= x0) {
                x1 = x0 + 1;
            }
            if (x1 > in_w) {
                x1 = in_w;
            }

            uint32_t sum = 0;
            uint32_t count = 0;

            for (int iy = y0; iy < y1; ++iy) {
                const uint8_t *row = src + (iy * in_w);
                for (int ix = x0; ix < x1; ++ix) {
                    sum += row[ix];
                    count++;
                }
            }

            if (count == 0) {
                out[(oy * out_w) + ox] = 0;
            } else {
                out[(oy * out_w) + ox] = (uint8_t)(sum / count);
            }
        }
    }

    return ESP_OK;
}