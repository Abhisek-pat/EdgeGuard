#include "inference_engine.h"

#include <math.h>
#include <string.h>

#include "app_config.h"
#include "camera_service.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "event_manager.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "inference_engine";

static TaskHandle_t s_inference_task_handle = NULL;
static inference_engine_status_t s_status = {
    .initialized = false,
    .running = false,
    .warmup_complete = false,
    .trigger_latched = false,
    .sample_count = 0,
    .trigger_count = 0,
    .dropped_frame_count = 0,
    .baseline_frame_len = 0,
    .last_frame_len = 0,
    .last_score = 0.0f,
    .trigger_threshold = EDGEGUARD_INFERENCE_TRIGGER_THRESHOLD,
    .last_inference_time_us = 0,
    .last_decision = "idle",
};

static float s_baseline_len = 0.0f;

static void set_decision(const char *value)
{
    strlcpy(s_status.last_decision, value, sizeof(s_status.last_decision));
}

static void inference_task(void *arg)
{
    vTaskDelay(pdMS_TO_TICKS(3000));
    (void)arg;
    s_status.running = true;

    while (1) {
        if (!camera_service_is_ready()) {
            set_decision("camera_not_ready");
            vTaskDelay(pdMS_TO_TICKS(EDGEGUARD_INFERENCE_PERIOD_MS));
            continue;
        }

        int64_t t0 = esp_timer_get_time();
        camera_fb_t *fb = camera_service_get_frame();

        if (fb == NULL) {
            s_status.dropped_frame_count++;
            set_decision("capture_failed");
            vTaskDelay(pdMS_TO_TICKS(EDGEGUARD_INFERENCE_PERIOD_MS));
            continue;
        }

        const size_t frame_len = fb->len;
        s_status.last_frame_len = frame_len;

        if (!s_status.warmup_complete) {
            if (s_status.sample_count == 0) {
                s_baseline_len = (float)frame_len;
            } else {
                s_baseline_len =
                    ((s_baseline_len * (float)s_status.sample_count) + (float)frame_len) /
                    (float)(s_status.sample_count + 1);
            }

            s_status.sample_count++;
            s_status.baseline_frame_len = (size_t)s_baseline_len;
            s_status.last_score = 0.0f;
            s_status.last_inference_time_us = esp_timer_get_time() - t0;

            if (s_status.sample_count >= EDGEGUARD_INFERENCE_WARMUP_FRAMES) {
                s_status.warmup_complete = true;
                set_decision("armed");
                ESP_LOGI(TAG, "warmup complete | baseline=%u bytes", (unsigned)s_status.baseline_frame_len);
            } else {
                set_decision("warming_up");
            }

            camera_service_return_frame(fb);
            vTaskDelay(pdMS_TO_TICKS(EDGEGUARD_INFERENCE_PERIOD_MS));
            continue;
        }

        const float baseline = (s_baseline_len > 1.0f) ? s_baseline_len : (float)frame_len;
        const float delta_ratio = fabsf((float)frame_len - baseline) / baseline;

        s_status.last_score = (0.70f * s_status.last_score) + (0.30f * delta_ratio);

        if (s_status.last_score < EDGEGUARD_INFERENCE_RESET_THRESHOLD) {
            s_baseline_len =
                (EDGEGUARD_INFERENCE_BASELINE_ALPHA * s_baseline_len) +
                ((1.0f - EDGEGUARD_INFERENCE_BASELINE_ALPHA) * (float)frame_len);
        }

        s_status.baseline_frame_len = (size_t)s_baseline_len;
        s_status.sample_count++;

        bool should_submit_event = false;

        if (!s_status.trigger_latched &&
            s_status.last_score >= EDGEGUARD_INFERENCE_TRIGGER_THRESHOLD) {
            s_status.trigger_latched = true;
            s_status.trigger_count++;
            should_submit_event = true;
            set_decision("triggered");
        } else if (s_status.trigger_latched &&
                   s_status.last_score <= EDGEGUARD_INFERENCE_RESET_THRESHOLD) {
            s_status.trigger_latched = false;
            set_decision("armed");
        } else {
            set_decision(s_status.trigger_latched ? "latched" : "monitoring");
        }

        s_status.last_inference_time_us = esp_timer_get_time() - t0;

        camera_service_return_frame(fb);

        if (should_submit_event) {
            float confidence = s_status.last_score;
            if (confidence > 1.0f) {
                confidence = 1.0f;
            }

            esp_err_t err = event_manager_submit_event("activity_detected", confidence, false);
            if (err != ESP_OK) {
                ESP_LOGW(TAG, "failed to submit live event");
            } else {
                ESP_LOGI(
                    TAG,
                    "live trigger | score=%.3f baseline=%u frame=%u triggers=%" PRIu32,
                    s_status.last_score,
                    (unsigned)s_status.baseline_frame_len,
                    (unsigned)s_status.last_frame_len,
                    s_status.trigger_count
                );
            }
        }

        vTaskDelay(pdMS_TO_TICKS(EDGEGUARD_INFERENCE_PERIOD_MS));
    }
}

esp_err_t inference_engine_init(void)
{
    if (s_status.initialized) {
        ESP_LOGW(TAG, "already initialized");
        return ESP_OK;
    }

    BaseType_t ok = xTaskCreate(
        inference_task,
        "edgeguard_infer",
        4096,
        NULL,
        5,
        &s_inference_task_handle
    );
    ESP_RETURN_ON_FALSE(ok == pdPASS, ESP_ERR_NO_MEM, TAG, "failed to create inference task");

    s_status.initialized = true;
    ESP_LOGI(TAG, "initialized");
    return ESP_OK;
}

const inference_engine_status_t *inference_engine_get_status(void)
{
    return &s_status;
}