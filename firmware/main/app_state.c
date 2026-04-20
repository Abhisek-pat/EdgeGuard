#include "app_state.h"
#include "app_config.h"

#include "esp_log.h"

static const char *TAG = "app_state";
static edgeguard_state_t g_state = {0};

static void lock_state(void)
{
    if (g_state.mutex != NULL) {
        xSemaphoreTake(g_state.mutex, portMAX_DELAY);
    }
}

static void unlock_state(void)
{
    if (g_state.mutex != NULL) {
        xSemaphoreGive(g_state.mutex);
    }
}

void app_state_init(void)
{
    g_state.mutex = xSemaphoreCreateMutex();
    g_state.threshold = EDGEGUARD_DEFAULT_THRESHOLD;
    g_state.boot_count = 1;
    g_state.event_count = 0;
    g_state.uptime_sec = 0;
    g_state.last_event_ms = 0;
    g_state.last_confidence = 0.0f;
    g_state.wifi_connected = false;
    g_state.mqtt_connected = false;
    g_state.alert_active = false;

    ESP_LOGI(TAG, "state initialized");
}

edgeguard_state_t *app_state_get(void)
{
    return &g_state;
}

void app_state_set_wifi_connected(bool value)
{
    lock_state();
    g_state.wifi_connected = value;
    unlock_state();
}

void app_state_set_mqtt_connected(bool value)
{
    lock_state();
    g_state.mqtt_connected = value;
    unlock_state();
}

void app_state_set_alert_active(bool value)
{
    lock_state();
    g_state.alert_active = value;
    unlock_state();
}

void app_state_set_threshold(float value)
{
    lock_state();
    g_state.threshold = value;
    unlock_state();
}

void app_state_set_last_confidence(float value)
{
    lock_state();
    g_state.last_confidence = value;
    unlock_state();
}

void app_state_increment_event_count(void)
{
    lock_state();
    g_state.event_count++;
    unlock_state();
}

void app_state_set_last_event_ms(uint32_t value)
{
    lock_state();
    g_state.last_event_ms = value;
    unlock_state();
}

void app_state_set_uptime_sec(uint32_t value)
{
    lock_state();
    g_state.uptime_sec = value;
    unlock_state();
}
