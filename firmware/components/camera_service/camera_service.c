#include "camera_service.h"

#include <inttypes.h>

#include "esp_check.h"
#include "esp_log.h"
#include "esp_psram.h"
#include "esp_timer.h"

static const char *TAG = "camera_service";

/*
 * AI-Thinker ESP32-CAM pin map
 */
#define CAM_PIN_PWDN   32
#define CAM_PIN_RESET  -1
#define CAM_PIN_XCLK   0
#define CAM_PIN_SIOD   26
#define CAM_PIN_SIOC   27

#define CAM_PIN_D7     35
#define CAM_PIN_D6     34
#define CAM_PIN_D5     39
#define CAM_PIN_D4     36
#define CAM_PIN_D3     21
#define CAM_PIN_D2     19
#define CAM_PIN_D1     18
#define CAM_PIN_D0     5
#define CAM_PIN_VSYNC  25
#define CAM_PIN_HREF   23
#define CAM_PIN_PCLK   22

static bool s_camera_ready = false;
static camera_service_status_t s_status = {
    .ready = false,
    .pixel_format = PIXFORMAT_GRAYSCALE,
    .frame_size = FRAMESIZE_QVGA,
    .fb_count = 1,
    .last_frame_len = 0,
    .last_width = 0,
    .last_height = 0,
    .last_capture_time_us = 0,
    .capture_count = 0,
};

static camera_config_t build_camera_config(bool has_psram)
{
    camera_config_t config = {
        .pin_pwdn = CAM_PIN_PWDN,
        .pin_reset = CAM_PIN_RESET,
        .pin_xclk = CAM_PIN_XCLK,
        .pin_sccb_sda = CAM_PIN_SIOD,
        .pin_sccb_scl = CAM_PIN_SIOC,

        .pin_d7 = CAM_PIN_D7,
        .pin_d6 = CAM_PIN_D6,
        .pin_d5 = CAM_PIN_D5,
        .pin_d4 = CAM_PIN_D4,
        .pin_d3 = CAM_PIN_D3,
        .pin_d2 = CAM_PIN_D2,
        .pin_d1 = CAM_PIN_D1,
        .pin_d0 = CAM_PIN_D0,
        .pin_vsync = CAM_PIN_VSYNC,
        .pin_href = CAM_PIN_HREF,
        .pin_pclk = CAM_PIN_PCLK,

        .xclk_freq_hz = 20000000,
        .ledc_timer = LEDC_TIMER_0,
        .ledc_channel = LEDC_CHANNEL_0,

        /*
         * 7B inference mode:
         * raw grayscale frames instead of JPEG
         */
        .pixel_format = PIXFORMAT_GRAYSCALE,
        .frame_size = has_psram ? FRAMESIZE_QVGA : FRAMESIZE_QQVGA,

        /*
         * jpeg_quality is ignored in grayscale mode, but keep a valid value.
         */
        .jpeg_quality = 12,

        /*
         * Raw grayscale is more memory-sensitive than JPEG.
         * Use a single framebuffer for stability.
         */
        .fb_count = 1,
        .grab_mode = CAMERA_GRAB_WHEN_EMPTY,
    };

    return config;
}

static void update_status_from_fb(const camera_fb_t *fb, int64_t capture_time_us)
{
    s_status.last_frame_len = fb->len;
    s_status.last_width = fb->width;
    s_status.last_height = fb->height;
    s_status.last_capture_time_us = capture_time_us;
    s_status.capture_count++;
}

esp_err_t camera_service_init(void)
{
    if (s_camera_ready) {
        ESP_LOGW(TAG, "camera already initialized");
        return ESP_OK;
    }

    const bool has_psram = esp_psram_is_initialized();
    ESP_LOGI(TAG, "PSRAM initialized: %s", has_psram ? "yes" : "no");

    camera_config_t config = build_camera_config(has_psram);

    ESP_LOGI(
        TAG,
        "camera config: pixel_format=%d frame_size=%d jpeg_quality=%d fb_count=%d",
        config.pixel_format,
        config.frame_size,
        config.jpeg_quality,
        config.fb_count
    );

    esp_err_t err = esp_camera_init(&config);
    ESP_RETURN_ON_ERROR(err, TAG, "esp_camera_init failed");

    sensor_t *sensor = esp_camera_sensor_get();
    if (sensor == NULL) {
        ESP_LOGE(TAG, "esp_camera_sensor_get returned NULL");
        return ESP_FAIL;
    }

    s_camera_ready = true;
    s_status.ready = true;
    s_status.pixel_format = config.pixel_format;
    s_status.frame_size = config.frame_size;
    s_status.fb_count = config.fb_count;

    ESP_LOGI(TAG, "camera initialized successfully");
    return ESP_OK;
}

camera_fb_t *camera_service_get_frame(void)
{
    if (!s_camera_ready) {
        ESP_LOGE(TAG, "camera not initialized");
        return NULL;
    }

    int64_t t0 = esp_timer_get_time();
    camera_fb_t *fb = esp_camera_fb_get();
    int64_t t1 = esp_timer_get_time();

    if (fb == NULL) {
        ESP_LOGE(TAG, "esp_camera_fb_get returned NULL");
        return NULL;
    }

    update_status_from_fb(fb, t1 - t0);
    return fb;
}

void camera_service_return_frame(camera_fb_t *fb)
{
    if (fb != NULL) {
        esp_camera_fb_return(fb);
    }
}

esp_err_t camera_service_capture_test(void)
{
    ESP_RETURN_ON_FALSE(s_camera_ready, ESP_ERR_INVALID_STATE, TAG, "camera not initialized");

    camera_fb_t *fb = camera_service_get_frame();
    if (fb == NULL) {
        ESP_LOGE(TAG, "camera capture test failed");
        return ESP_FAIL;
    }

    ESP_LOGI(
        TAG,
        "capture ok: %ux%u len=%u format=%d time=%" PRId64 " us",
        fb->width,
        fb->height,
        (unsigned)fb->len,
        fb->format,
        s_status.last_capture_time_us
    );

    camera_service_return_frame(fb);
    return ESP_OK;
}

bool camera_service_is_ready(void)
{
    return s_camera_ready;
}

const camera_service_status_t *camera_service_get_status(void)
{
    return &s_status;
}