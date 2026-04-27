#include "event_manager.h"

#include <inttypes.h>
#include <string.h>

#include "app_config.h"
#include "app_state.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "led_alert.h"
#include "mqtt_service.h"

static const char *TAG = "event_manager";

typedef struct
{
    char event_name[32];
    float confidence;
    bool is_test;
} edgeguard_event_t;

static QueueHandle_t s_event_queue = NULL;
static TaskHandle_t s_event_task_handle = NULL;

static event_manager_status_t s_status = {
    .initialized = false,
    .accepted_events = 0,
    .ignored_events = 0,
    .last_event_ts_ms = 0,
    .last_event_confidence = 0.0f,
    .last_event_name = {0},
};

static edgeguard_recent_event_t s_history[EDGEGUARD_EVENT_HISTORY_LEN];
static size_t s_history_count = 0;
static size_t s_history_write_idx = 0;

static bool cooldown_active(uint64_t now_ms)
{
    if (s_status.last_event_ts_ms == 0) {
        return false;
    }
    return (now_ms - s_status.last_event_ts_ms) < EDGEGUARD_EVENT_COOLDOWN_MS;
}

static void record_recent_event(
    const char *name,
    float confidence,
    bool accepted,
    bool is_test,
    uint64_t timestamp_ms)
{
    edgeguard_recent_event_t *slot = &s_history[s_history_write_idx];

    memset(slot, 0, sizeof(*slot));
    strlcpy(slot->name, name, sizeof(slot->name));
    slot->confidence = confidence;
    slot->accepted = accepted;
    slot->is_test = is_test;
    slot->timestamp_ms = timestamp_ms;

    s_history_write_idx = (s_history_write_idx + 1) % EDGEGUARD_EVENT_HISTORY_LEN;
    if (s_history_count < EDGEGUARD_EVENT_HISTORY_LEN) {
        s_history_count++;
    }
}

static void event_manager_task(void *arg)
{
    (void)arg;

    edgeguard_event_t event;

    while (1) {
        if (xQueueReceive(s_event_queue, &event, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        const uint64_t now_ms = (uint64_t)(esp_timer_get_time() / 1000ULL);

        if (cooldown_active(now_ms)) {
            s_status.ignored_events++;

            record_recent_event(
                event.event_name,
                event.confidence,
                false,
                event.is_test,
                now_ms
            );

            ESP_LOGW(
                TAG,
                "event ignored due to cooldown | name=%s confidence=%.2f ignored=%" PRIu32,
                event.event_name,
                event.confidence,
                s_status.ignored_events
            );
            continue;
        }

        s_status.accepted_events++;
        s_status.last_event_ts_ms = now_ms;
        s_status.last_event_confidence = event.confidence;
        strlcpy(s_status.last_event_name, event.event_name, sizeof(s_status.last_event_name));

        record_recent_event(
            event.event_name,
            event.confidence,
            true,
            event.is_test,
            now_ms
        );

        app_state_increment_event_count();
        app_state_set_last_event_ms((uint32_t)now_ms);
        app_state_set_last_confidence(event.confidence);
        app_state_set_alert_active(true);

        ESP_LOGI(
            TAG,
            "event accepted | name=%s confidence=%.2f test=%d accepted=%" PRIu32,
            event.event_name,
            event.confidence,
            event.is_test,
            s_status.accepted_events
        );

        if (led_alert_blink(
                EDGEGUARD_ALERT_LED_ON_MS,
                EDGEGUARD_ALERT_LED_OFF_MS,
                EDGEGUARD_ALERT_LED_BLINK_COUNT) != ESP_OK) {
            ESP_LOGE(TAG, "led alert failed");
        }

        if (mqtt_service_is_connected()) {
            if (mqtt_service_publish_event(event.event_name, event.confidence) != ESP_OK) {
                ESP_LOGE(TAG, "mqtt publish event failed");
            }
        } else {
            ESP_LOGW(TAG, "mqtt not connected, skipping event publish");
        }

        app_state_set_alert_active(false);
    }
}

esp_err_t event_manager_submit_event(const char *event_name, float confidence, bool is_test)
{
    ESP_RETURN_ON_FALSE(s_status.initialized, ESP_ERR_INVALID_STATE, TAG, "event_manager not initialized");
    ESP_RETURN_ON_FALSE(event_name != NULL, ESP_ERR_INVALID_ARG, TAG, "event_name is null");

    edgeguard_event_t event = {0};
    strlcpy(event.event_name, event_name, sizeof(event.event_name));
    event.confidence = confidence;
    event.is_test = is_test;

    BaseType_t ok = xQueueSend(s_event_queue, &event, pdMS_TO_TICKS(100));
    ESP_RETURN_ON_FALSE(ok == pdTRUE, ESP_ERR_TIMEOUT, TAG, "failed to queue event");

    return ESP_OK;
}

esp_err_t event_manager_trigger_test_event(void)
{
    return event_manager_submit_event("person_detected", 0.99f, true);
}

esp_err_t event_manager_init(void)
{
    if (s_status.initialized) {
        ESP_LOGW(TAG, "already initialized");
        return ESP_OK;
    }

    s_event_queue = xQueueCreate(EDGEGUARD_EVENT_QUEUE_LEN, sizeof(edgeguard_event_t));
    ESP_RETURN_ON_FALSE(s_event_queue != NULL, ESP_ERR_NO_MEM, TAG, "failed to create event queue");

    BaseType_t ok = xTaskCreate(
        event_manager_task,
        "edgeguard_event",
        4096,
        NULL,
        5,
        &s_event_task_handle
    );
    ESP_RETURN_ON_FALSE(ok == pdPASS, ESP_ERR_NO_MEM, TAG, "failed to create event task");

    s_status.initialized = true;
    ESP_LOGI(TAG, "initialized");
    return ESP_OK;
}

const event_manager_status_t *event_manager_get_status(void)
{
    return &s_status;
}

size_t event_manager_get_recent_events(edgeguard_recent_event_t *out_events, size_t max_items)
{
    if (out_events == NULL || max_items == 0) {
        return 0;
    }

    const size_t n = (s_history_count < max_items) ? s_history_count : max_items;

    for (size_t i = 0; i < n; ++i) {
        size_t idx = (s_history_write_idx + EDGEGUARD_EVENT_HISTORY_LEN - 1 - i) % EDGEGUARD_EVENT_HISTORY_LEN;
        out_events[i] = s_history[idx];
    }

    return n;
}