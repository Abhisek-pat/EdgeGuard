#include "mqtt_service.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "app_config.h"
#include "camera_service.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mqtt_client.h"
#include "network_manager.h"

static const char *TAG = "mqtt_service";

static esp_mqtt_client_handle_t s_client = NULL;
static mqtt_service_status_t s_status = {
    .initialized = false,
    .started = false,
    .connected = false,
    .boot_event_sent = false,
    .publish_count = 0,
    .broker_uri = {0},
    .client_id = {0},
    .base_topic = {0},
};

static TaskHandle_t s_mqtt_task_handle = NULL;

static bool mqtt_config_looks_unconfigured(void)
{
    return (strlen(EDGEGUARD_MQTT_BROKER_URI) == 0) ||
           (strcmp(EDGEGUARD_MQTT_BROKER_URI, "mqtt://YOUR_BROKER_IP:1883") == 0);
}

static void build_topic(char *out, size_t out_size, const char *suffix)
{
    snprintf(out, out_size, "%s/%s", s_status.base_topic, suffix);
}

static esp_err_t enqueue_json(const char *topic, const char *payload)
{
    ESP_RETURN_ON_FALSE(s_client != NULL, ESP_ERR_INVALID_STATE, TAG, "mqtt client not initialized");
    ESP_RETURN_ON_FALSE(topic != NULL, ESP_ERR_INVALID_ARG, TAG, "topic is null");
    ESP_RETURN_ON_FALSE(payload != NULL, ESP_ERR_INVALID_ARG, TAG, "payload is null");

    int msg_id = esp_mqtt_client_enqueue(s_client, topic, payload, 0, 1, 0, true);
    ESP_RETURN_ON_FALSE(msg_id >= 0, ESP_FAIL, TAG, "failed to enqueue mqtt message");

    s_status.publish_count++;
    ESP_LOGI(TAG, "queued topic=%s msg_id=%d", topic, msg_id);
    return ESP_OK;
}

static void mqtt_event_handler(
    void *handler_args,
    esp_event_base_t base,
    int32_t event_id,
    void *event_data)
{
    (void)handler_args;
    (void)base;

    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;

    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_BEFORE_CONNECT:
            ESP_LOGI(TAG, "connecting to broker: %s", s_status.broker_uri);
            break;

        case MQTT_EVENT_CONNECTED:
            s_status.connected = true;
            ESP_LOGI(TAG, "MQTT connected");
            break;

        case MQTT_EVENT_DISCONNECTED:
            s_status.connected = false;
            ESP_LOGW(TAG, "MQTT disconnected");
            break;

        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "MQTT published msg_id=%d", event ? event->msg_id : -1);
            break;

        case MQTT_EVENT_ERROR:
            s_status.connected = false;
            ESP_LOGE(TAG, "MQTT error");
            break;

        default:
            break;
    }
}

esp_err_t mqtt_service_publish_health(void)
{
    const network_manager_status_t *net = network_manager_get_status();
    const camera_service_status_t *cam = camera_service_get_status();

    char topic[160];
    char payload[512];

    build_topic(topic, sizeof(topic), "health");

    snprintf(
        payload,
        sizeof(payload),
        "{"
        "\"device_id\":\"%s\","
        "\"project\":\"%s\","
        "\"fw_version\":\"%s\","
        "\"uptime_sec\":%" PRIu64 ","
        "\"wifi_connected\":%s,"
        "\"ip_addr\":\"%s\","
        "\"camera_ready\":%s,"
        "\"frame_width\":%u,"
        "\"frame_height\":%u,"
        "\"last_frame_len\":%u,"
        "\"last_capture_time_us\":%" PRId64
        "}",
        EDGEGUARD_DEVICE_ID,
        EDGEGUARD_PROJECT_NAME,
        EDGEGUARD_FW_VERSION,
        (uint64_t)(esp_timer_get_time() / 1000000ULL),
        net->connected ? "true" : "false",
        net->ip_addr,
        cam->ready ? "true" : "false",
        cam->last_width,
        cam->last_height,
        (unsigned)cam->last_frame_len,
        cam->last_capture_time_us
    );

    return enqueue_json(topic, payload);
}

esp_err_t mqtt_service_publish_event(const char *event_name, float confidence)
{
    char topic[160];
    char payload[320];

    ESP_RETURN_ON_FALSE(event_name != NULL, ESP_ERR_INVALID_ARG, TAG, "event_name is null");

    build_topic(topic, sizeof(topic), "events");

    snprintf(
        payload,
        sizeof(payload),
        "{"
        "\"device_id\":\"%s\","
        "\"event\":\"%s\","
        "\"confidence\":%.2f,"
        "\"timestamp_us\":%" PRIu64
        "}",
        EDGEGUARD_DEVICE_ID,
        event_name,
        confidence,
        (uint64_t)esp_timer_get_time()
    );

    return enqueue_json(topic, payload);
}

static void mqtt_service_task(void *arg)
{
    (void)arg;
    bool tried_start = false;

    while (1) {
        const bool wifi_ready = network_manager_is_connected();

        if (wifi_ready && s_client != NULL && !tried_start) {
            if (esp_mqtt_client_start(s_client) == ESP_OK) {
                s_status.started = true;
                tried_start = true;
                ESP_LOGI(TAG, "mqtt client started");
            } else {
                ESP_LOGE(TAG, "failed to start mqtt client");
            }
        }

        if (!wifi_ready) {
            tried_start = false;
            s_status.connected = false;
        }

        if (wifi_ready && s_status.connected) {
            if (!s_status.boot_event_sent) {
                if (mqtt_service_publish_event("boot", 1.0f) == ESP_OK) {
                    s_status.boot_event_sent = true;
                }
            }

            mqtt_service_publish_health();
        }

        vTaskDelay(pdMS_TO_TICKS(EDGEGUARD_MQTT_HEALTH_PUBLISH_PERIOD_MS));
    }
}

esp_err_t mqtt_service_init(void)
{
    if (s_status.initialized) {
        ESP_LOGW(TAG, "already initialized");
        return ESP_OK;
    }

    if (mqtt_config_looks_unconfigured()) {
        ESP_LOGW(TAG, "MQTT broker URI not configured. Update EDGEGUARD_MQTT_BROKER_URI in app_config.h");
        s_status.initialized = true;
        return ESP_OK;
    }

    strlcpy(s_status.broker_uri, EDGEGUARD_MQTT_BROKER_URI, sizeof(s_status.broker_uri));
    strlcpy(s_status.client_id, EDGEGUARD_MQTT_CLIENT_ID, sizeof(s_status.client_id));

    snprintf(
        s_status.base_topic,
        sizeof(s_status.base_topic),
        "%s/%s",
        EDGEGUARD_MQTT_TOPIC_ROOT,
        EDGEGUARD_DEVICE_ID
    );

    const esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = EDGEGUARD_MQTT_BROKER_URI,
        .credentials.client_id = EDGEGUARD_MQTT_CLIENT_ID,
        .credentials.username = EDGEGUARD_MQTT_USERNAME,
        .credentials.authentication.password = EDGEGUARD_MQTT_PASSWORD,
        .network.reconnect_timeout_ms = 5000,
    };

    s_client = esp_mqtt_client_init(&mqtt_cfg);
    ESP_RETURN_ON_FALSE(s_client != NULL, ESP_FAIL, TAG, "esp_mqtt_client_init failed");

    ESP_RETURN_ON_ERROR(
        esp_mqtt_client_register_event(s_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL),
        TAG,
        "esp_mqtt_client_register_event failed"
    );

    BaseType_t ok = xTaskCreate(
        mqtt_service_task,
        "edgeguard_mqtt",
        4096,
        NULL,
        4,
        &s_mqtt_task_handle
    );
    ESP_RETURN_ON_FALSE(ok == pdPASS, ESP_ERR_NO_MEM, TAG, "failed to create mqtt task");

    s_status.initialized = true;

    ESP_LOGI(TAG, "mqtt service initialized for broker %s", s_status.broker_uri);
    return ESP_OK;
}

bool mqtt_service_is_connected(void)
{
    return s_status.connected;
}

const mqtt_service_status_t *mqtt_service_get_status(void)
{
    return &s_status;
}