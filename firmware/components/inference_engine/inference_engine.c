#include "inference_engine.h"

#include <inttypes.h>
#include <string.h>

#include "app_config.h"
#include "camera_service.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "event_manager.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "model_runner.h"
#include "preprocess.h"

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
    .trigger_threshold = EDGEGUARD_PERSON_DETECT_THRESHOLD,
    .last_inference_time_us = 0,
    .last_decision = "idle",
};

static uint8_t s_model_input[EDGEGUARD_MODEL_INPUT_WIDTH * EDGEGUARD_MODEL_INPUT_HEIGHT];

static void set_decision(const char *value)
{
    strlcpy(s_status.last_decision, value, sizeof(s_status.last_decision));
}

static float clamp01(float x)
{
    if (x < 0.0f) {
        return 0.0f;
    }
    if (x > 1.0f) {
        return 1.0f;
    }
    return x;
}

static void inference_task(void *arg)
{
    (void)arg;

    static bool did_model_smoke_test = false;
    static uint8_t smoke_test_input[EDGEGUARD_MODEL_INPUT_WIDTH * EDGEGUARD_MODEL_INPUT_HEIGHT];

    s_status.running = true;

    /*
     * Let camera and Wi-Fi settle before continuous inference.
     */
    vTaskDelay(pdMS_TO_TICKS(3000));

    while (1) {
        /*
         * One-time TFLM smoke test.
         * This proves the model can invoke on-device before we rely on live frames.
         */
        if (!did_model_smoke_test) {
            memset(smoke_test_input, 0, sizeof(smoke_test_input));

            float smoke_score = 0.0f;
            esp_err_t smoke_err = model_runner_infer_u8(
                smoke_test_input,
                sizeof(smoke_test_input),
                &smoke_score
            );

            if (smoke_err == ESP_OK) {
                ESP_LOGI(TAG, "model smoke test ok | score=%.4f", smoke_score);
                did_model_smoke_test = true;
            } else {
                ESP_LOGE(TAG, "model smoke test failed");
                set_decision("model_error");
                vTaskDelay(pdMS_TO_TICKS(EDGEGUARD_INFERENCE_PERIOD_MS));
                continue;
            }
        }

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

        s_status.last_frame_len = fb->len;

        esp_err_t prep_err = preprocess_resize_grayscale(
            fb,
            s_model_input,
            sizeof(s_model_input),
            EDGEGUARD_MODEL_INPUT_WIDTH,
            EDGEGUARD_MODEL_INPUT_HEIGHT
        );

        if (prep_err != ESP_OK) {
            camera_service_return_frame(fb);
            s_status.dropped_frame_count++;
            set_decision("preprocess_failed");
            vTaskDelay(pdMS_TO_TICKS(EDGEGUARD_INFERENCE_PERIOD_MS));
            continue;
        }

        float person_score = 0.0f;
        esp_err_t infer_err = model_runner_infer_u8(
            s_model_input,
            sizeof(s_model_input),
            &person_score
        );

        s_status.last_inference_time_us = esp_timer_get_time() - t0;

        camera_service_return_frame(fb);

        if (infer_err != ESP_OK) {
            s_status.dropped_frame_count++;
            set_decision("infer_failed");
            vTaskDelay(pdMS_TO_TICKS(EDGEGUARD_INFERENCE_PERIOD_MS));
            continue;
        }

        person_score = clamp01(person_score);
        s_status.last_score = person_score;
        s_status.sample_count++;
        s_status.baseline_frame_len = 0; /* no longer used in model-based path */

        /*
         * Warmup phase:
         * ignore early frames while exposure and scene settle.
         */
        if (!s_status.warmup_complete) {
            if (s_status.sample_count >= EDGEGUARD_INFERENCE_WARMUP_FRAMES) {
                s_status.warmup_complete = true;
                set_decision("armed");
                ESP_LOGI(TAG, "model warmup complete | threshold=%.2f",
                         EDGEGUARD_PERSON_DETECT_THRESHOLD);
            } else {
                set_decision("warming_up");
            }

            vTaskDelay(pdMS_TO_TICKS(EDGEGUARD_INFERENCE_PERIOD_MS));
            continue;
        }

        bool should_submit_event = false;

        if (!s_status.trigger_latched &&
            person_score >= EDGEGUARD_PERSON_DETECT_THRESHOLD) {
            s_status.trigger_latched = true;
            s_status.trigger_count++;
            should_submit_event = true;
            set_decision("triggered");
        } else if (s_status.trigger_latched &&
                   person_score <= EDGEGUARD_PERSON_RESET_THRESHOLD) {
            s_status.trigger_latched = false;
            set_decision("armed");
        } else {
            set_decision(s_status.trigger_latched ? "latched" : "monitoring");
        }

        if (should_submit_event) {
            esp_err_t err = event_manager_submit_event("person_detected", person_score, false);
            if (err != ESP_OK) {
                ESP_LOGW(TAG, "failed to submit person_detected event");
            } else {
                ESP_LOGI(
                    TAG,
                    "person trigger | score=%.3f threshold=%.2f triggers=%" PRIu32,
                    person_score,
                    EDGEGUARD_PERSON_DETECT_THRESHOLD,
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

    ESP_RETURN_ON_ERROR(model_runner_init(), TAG, "model_runner_init failed");

    model_runner_info_t info = {0};
    ESP_RETURN_ON_ERROR(model_runner_get_info(&info), TAG, "model_runner_get_info failed");

    ESP_RETURN_ON_FALSE(
        info.input_width == EDGEGUARD_MODEL_INPUT_WIDTH &&
        info.input_height == EDGEGUARD_MODEL_INPUT_HEIGHT &&
        info.input_channels == 1,
        ESP_FAIL,
        TAG,
        "model input shape mismatch"
    );

    s_status.trigger_threshold = EDGEGUARD_PERSON_DETECT_THRESHOLD;

    BaseType_t ok = xTaskCreate(
        inference_task,
        "edgeguard_infer",
        6144,
        NULL,
        5,
        &s_inference_task_handle
    );
    ESP_RETURN_ON_FALSE(ok == pdPASS, ESP_ERR_NO_MEM, TAG, "failed to create inference task");

    s_status.initialized = true;
    ESP_LOGI(
        TAG,
        "initialized | model_input=%dx%dx%d threshold=%.2f",
        info.input_width,
        info.input_height,
        info.input_channels,
        s_status.trigger_threshold
    );
    return ESP_OK;
}

const inference_engine_status_t *inference_engine_get_status(void)
{
    return &s_status;
}