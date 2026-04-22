#include <inttypes.h>
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_err.h"
#include "esp_log.h"

#include "app_config.h"
#include "app_state.h"

#include "camera_service.h"
#include "event_manager.h"
#include "inference_engine.h"
#include "led_alert.h"
#include "mqtt_service.h"
#include "network_manager.h"
#include "storage.h"
#include "web_server.h"
#include "esp_check.h"
#include "event_manager.h"
#include "inference_engine.h"

static const char *TAG = "edgeguard_main";

static void heartbeat_task(void *arg)
{
    (void)arg;

    while (1) {
        app_state_set_wifi_connected(network_manager_is_connected());
        app_state_set_mqtt_connected(mqtt_service_is_connected());
        edgeguard_state_t *state = app_state_get();
        app_state_set_uptime_sec((uint32_t)(xTaskGetTickCount() / configTICK_RATE_HZ));

        const event_manager_status_t *evt = event_manager_get_status();
        const inference_engine_status_t *infer = inference_engine_get_status();

        ESP_LOGI(
            TAG,
            "heartbeat | wifi=%d mqtt=%d alert=%d events=%" PRIu32 " last_event=%s score=%.3f decision=%s uptime=%" PRIu32 "s",
            state->wifi_connected,
            state->mqtt_connected,
            state->alert_active,
            state->event_count,
            evt->last_event_name[0] ? evt->last_event_name : "none",
            infer->last_score,
            infer->last_decision,
            state->uptime_sec
        );

        vTaskDelay(pdMS_TO_TICKS(EDGEGUARD_HEARTBEAT_PERIOD_MS));
    }
}

static esp_err_t init_all_services(void)
{
    ESP_RETURN_ON_ERROR(storage_init(), TAG, "storage init failed");
    ESP_RETURN_ON_ERROR(led_alert_init(), TAG, "led_alert init failed");
    ESP_RETURN_ON_ERROR(camera_service_init(), TAG, "camera_service init failed");
    ESP_RETURN_ON_ERROR(camera_service_capture_test(), TAG, "camera capture self-test failed");
    ESP_RETURN_ON_ERROR(event_manager_init(), TAG, "event_manager init failed");
    ESP_RETURN_ON_ERROR(inference_engine_init(), TAG, "inference_engine init failed");
    ESP_RETURN_ON_ERROR(network_manager_init(), TAG, "network_manager init failed");
    ESP_RETURN_ON_ERROR(mqtt_service_init(), TAG, "mqtt_service init failed");
    ESP_RETURN_ON_ERROR(web_server_init(), TAG, "web_server init failed");

    return ESP_OK;
}

void app_main(void)
{
    ESP_LOGI(TAG, "booting %s v%s", EDGEGUARD_PROJECT_NAME, EDGEGUARD_FW_VERSION);

    app_state_init();

    ESP_ERROR_CHECK(init_all_services());

    xTaskCreate(
        heartbeat_task,
        "edgeguard_heartbeat",
        4096,
        NULL,
        5,
        NULL
    );

    ESP_LOGI(TAG, "system initialized");
}
