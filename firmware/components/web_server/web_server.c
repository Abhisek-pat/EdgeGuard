#include "web_server.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "app_config.h"
#include "app_state.h"
#include "camera_service.h"
#include "esp_check.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "event_manager.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "inference_engine.h"
#include "mqtt_service.h"
#include "network_manager.h"

static const char *TAG = "web_server";

static httpd_handle_t s_server = NULL;
static TaskHandle_t s_web_task_handle = NULL;

static const char *INDEX_HTML =
"<!doctype html>"
"<html><head>"
"<meta charset='utf-8'>"
"<meta name='viewport' content='width=device-width,initial-scale=1'>"
"<title>EdgeGuard Dashboard</title>"
"<style>"
"body{font-family:Arial,sans-serif;background:#101418;color:#eaf2f8;margin:0;padding:24px;}"
".card{background:#182028;border-radius:14px;padding:16px;margin-bottom:16px;box-shadow:0 4px 18px rgba(0,0,0,.25);}"
".title{font-size:28px;font-weight:700;margin-bottom:6px;}"
".sub{color:#9fb3c8;margin-bottom:20px;}"
".grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(220px,1fr));gap:12px;}"
".label{font-size:12px;color:#8ea3b8;text-transform:uppercase;letter-spacing:.08em;}"
".value{font-size:20px;font-weight:700;margin-top:6px;word-break:break-word;}"
"button{background:#2f81f7;color:#fff;border:none;padding:12px 16px;border-radius:10px;font-size:15px;cursor:pointer;}"
"button:hover{opacity:.92;}"
"</style>"
"</head><body>"
"<div class='title'>EdgeGuard</div>"
"<div class='sub'>ESP32-CAM live dashboard</div>"
"<div class='grid'>"
"<div class='card'><div class='label'>Wi-Fi</div><div class='value' id='wifi'>loading...</div></div>"
"<div class='card'><div class='label'>IP Address</div><div class='value' id='ip'>loading...</div></div>"
"<div class='card'><div class='label'>Uptime</div><div class='value' id='uptime'>loading...</div></div>"
"<div class='card'><div class='label'>MQTT</div><div class='value' id='mqtt'>loading...</div></div>"
"<div class='card'><div class='label'>Camera Ready</div><div class='value' id='camera'>loading...</div></div>"
"<div class='card'><div class='label'>Last Frame</div><div class='value' id='frame'>loading...</div></div>"
"<div class='card'><div class='label'>Capture Time</div><div class='value' id='capture'>loading...</div></div>"
"<div class='card'><div class='label'>Capture Count</div><div class='value' id='capture_count'>loading...</div></div>"
"<div class='card'><div class='label'>Inference Score</div><div class='value' id='infer_score'>loading...</div></div>"
"<div class='card'><div class='label'>Inference Decision</div><div class='value' id='infer_decision'>loading...</div></div>"
"<div class='card'><div class='label'>Trigger Latched</div><div class='value' id='infer_latched'>loading...</div></div>"
"<div class='card'><div class='label'>Baseline Bytes</div><div class='value' id='infer_baseline'>loading...</div></div>"
"<div class='card'><div class='label'>Event Count</div><div class='value' id='event_count'>loading...</div></div>"
"<div class='card'><div class='label'>Last Event</div><div class='value' id='last_event'>loading...</div></div>"
"<div class='card'><div class='label'>Last Confidence</div><div class='value' id='last_conf'>loading...</div></div>"
"<div class='card'><div class='label'>Alert Active</div><div class='value' id='alert_active'>loading...</div></div>"
"</div>"
"<div class='card'>"
"<div class='label'>Test Controls</div>"
"<div class='value'><button onclick='triggerTestEvent()'>Trigger Test Event</button></div>"
"</div>"
"<script>"
"async function refreshStatus(){"
"  try{"
"    const r=await fetch('/api/status',{cache:'no-store'});"
"    const s=await r.json();"
"    document.getElementById('wifi').textContent=s.wifi_connected ? ('Connected to ' + s.ssid) : 'Disconnected';"
"    document.getElementById('ip').textContent=s.ip_addr || 'N/A';"
"    document.getElementById('uptime').textContent=s.uptime_sec + ' s';"
"    document.getElementById('mqtt').textContent=s.mqtt_connected ? 'Connected' : 'Disconnected';"
"    document.getElementById('camera').textContent=s.camera_ready ? 'Yes' : 'No';"
"    document.getElementById('frame').textContent=s.frame_width + 'x' + s.frame_height + ' (' + s.last_frame_len + ' bytes)';"
"    document.getElementById('capture').textContent=s.last_capture_time_us + ' us';"
"    document.getElementById('capture_count').textContent=s.capture_count;"
"    document.getElementById('infer_score').textContent=Number(s.inference_score).toFixed(3);"
"    document.getElementById('infer_decision').textContent=s.inference_decision;"
"    document.getElementById('infer_latched').textContent=s.trigger_latched ? 'Yes' : 'No';"
"    document.getElementById('infer_baseline').textContent=s.baseline_frame_len + ' bytes';"
"    document.getElementById('event_count').textContent=s.event_count;"
"    document.getElementById('last_event').textContent=s.last_event_name || 'none';"
"    document.getElementById('last_conf').textContent=Number(s.last_confidence).toFixed(2);"
"    document.getElementById('alert_active').textContent=s.alert_active ? 'Yes' : 'No';"
"  }catch(e){"
"    document.getElementById('wifi').textContent='status fetch failed';"
"  }"
"}"
"async function triggerTestEvent(){"
"  try{"
"    await fetch('/api/test_event',{method:'POST'});"
"    setTimeout(refreshStatus, 300);"
"  }catch(e){"
"    console.log('test event failed', e);"
"  }"
"}"
"refreshStatus();"
"setInterval(refreshStatus, 2000);"
"</script>"
"</body></html>";

static esp_err_t root_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, INDEX_HTML, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t status_get_handler(httpd_req_t *req)
{
    const network_manager_status_t *net = network_manager_get_status();
    const camera_service_status_t *cam = camera_service_get_status();
    const mqtt_service_status_t *mqtt = mqtt_service_get_status();
    const event_manager_status_t *evt = event_manager_get_status();
    const inference_engine_status_t *infer = inference_engine_get_status();
    const edgeguard_state_t *state = app_state_get();
    const uint32_t uptime_sec = (uint32_t)(esp_timer_get_time() / 1000000ULL);

    char response[1024];

    snprintf(
        response,
        sizeof(response),
        "{"
        "\"project\":\"%s\","
        "\"fw_version\":\"%s\","
        "\"device_id\":\"%s\","
        "\"uptime_sec\":%" PRIu32 ","
        "\"wifi_connected\":%s,"
        "\"ssid\":\"%s\","
        "\"ip_addr\":\"%s\","
        "\"mqtt_connected\":%s,"
        "\"mqtt_broker\":\"%s\","
        "\"camera_ready\":%s,"
        "\"frame_width\":%u,"
        "\"frame_height\":%u,"
        "\"last_frame_len\":%u,"
        "\"last_capture_time_us\":%" PRId64 ","
        "\"capture_count\":%" PRIu32 ","
        "\"inference_score\":%.3f,"
        "\"inference_decision\":\"%s\","
        "\"trigger_latched\":%s,"
        "\"baseline_frame_len\":%u,"
        "\"inference_samples\":%" PRIu32 ","
        "\"inference_triggers\":%" PRIu32 ","
        "\"last_inference_time_us\":%" PRId64 ","
        "\"event_count\":%" PRIu32 ","
        "\"last_event_name\":\"%s\","
        "\"last_event_ts_ms\":%" PRIu64 ","
        "\"last_confidence\":%.2f,"
        "\"alert_active\":%s,"
        "\"accepted_events\":%" PRIu32 ","
        "\"ignored_events\":%" PRIu32
        "}",
        EDGEGUARD_PROJECT_NAME,
        EDGEGUARD_FW_VERSION,
        EDGEGUARD_DEVICE_ID,
        uptime_sec,
        net->connected ? "true" : "false",
        net->ssid,
        net->ip_addr,
        mqtt->connected ? "true" : "false",
        mqtt->broker_uri,
        cam->ready ? "true" : "false",
        cam->last_width,
        cam->last_height,
        (unsigned)cam->last_frame_len,
        cam->last_capture_time_us,
        cam->capture_count,
        (double)infer->last_score,
        infer->last_decision,
        infer->trigger_latched ? "true" : "false",
        (unsigned)infer->baseline_frame_len,
        infer->sample_count,
        infer->trigger_count,
        infer->last_inference_time_us,
        state->event_count,
        evt->last_event_name,
        evt->last_event_ts_ms,
        (double)state->last_confidence,
        state->alert_active ? "true" : "false",
        evt->accepted_events,
        evt->ignored_events
    );

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_sendstr(req, response);
}

static esp_err_t test_event_post_handler(httpd_req_t *req)
{
    esp_err_t err = event_manager_trigger_test_event();

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");

    if (err == ESP_OK) {
        return httpd_resp_sendstr(req, "{\"ok\":true,\"message\":\"test event queued\"}");
    }

    return httpd_resp_sendstr(req, "{\"ok\":false,\"message\":\"failed to queue test event\"}");
}

static esp_err_t start_http_server(void)
{
    if (s_server != NULL) {
        return ESP_OK;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = EDGEGUARD_HTTP_SERVER_PORT;
    config.max_uri_handlers = 10;
    config.stack_size = 10240;

    ESP_RETURN_ON_ERROR(httpd_start(&s_server, &config), TAG, "httpd_start failed");

    httpd_uri_t root = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = root_get_handler,
        .user_ctx = NULL
    };

    httpd_uri_t status = {
        .uri = "/api/status",
        .method = HTTP_GET,
        .handler = status_get_handler,
        .user_ctx = NULL
    };

    httpd_uri_t test_event = {
        .uri = "/api/test_event",
        .method = HTTP_POST,
        .handler = test_event_post_handler,
        .user_ctx = NULL
    };

    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_server, &root), TAG, "register / failed");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_server, &status), TAG, "register /api/status failed");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_server, &test_event), TAG, "register /api/test_event failed");

    ESP_LOGI(TAG, "dashboard started on http://%s:%d", network_manager_get_ip_addr(), EDGEGUARD_HTTP_SERVER_PORT);
    return ESP_OK;
}

static void stop_http_server(void)
{
    if (s_server != NULL) {
        ESP_LOGI(TAG, "stopping dashboard");
        httpd_stop(s_server);
        s_server = NULL;
    }
}

static void web_server_task(void *arg)
{
    (void)arg;

    while (1) {
        const bool connected = network_manager_is_connected();

        if (connected && s_server == NULL) {
            if (start_http_server() != ESP_OK) {
                ESP_LOGE(TAG, "failed to start dashboard");
            }
        } else if (!connected && s_server != NULL) {
            stop_http_server();
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

esp_err_t web_server_init(void)
{
    if (s_web_task_handle != NULL) {
        ESP_LOGW(TAG, "already initialized");
        return ESP_OK;
    }

    BaseType_t ok = xTaskCreate(
        web_server_task,
        "edgeguard_web",
        6144,
        NULL,
        4,
        &s_web_task_handle
    );

    ESP_RETURN_ON_FALSE(ok == pdPASS, ESP_ERR_NO_MEM, TAG, "failed to create web server task");

    ESP_LOGI(TAG, "dashboard task initialized");
    return ESP_OK;
}