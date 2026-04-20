#include "network_manager.h"

#include <string.h>

#include "app_config.h"
#include "esp_check.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_ip_addr.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

static const char *TAG = "network_manager";

static network_manager_status_t s_status = {
    .initialized = false,
    .started = false,
    .connected = false,
    .retry_count = 0,
    .ssid = {0},
    .ip_addr = {0},
};

static esp_netif_t *s_sta_netif = NULL;
static esp_event_handler_instance_t s_any_wifi_handler;
static esp_event_handler_instance_t s_got_ip_handler;

static bool credentials_look_unconfigured(void)
{
    return (strlen(EDGEGUARD_WIFI_SSID) == 0) ||
           (strcmp(EDGEGUARD_WIFI_SSID, "YOUR_WIFI_SSID") == 0);
}

static void wifi_event_handler(
    void *arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void *event_data)
{
    (void)arg;
    (void)event_base;

    if (event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "Wi-Fi started, connecting to SSID: %s", EDGEGUARD_WIFI_SSID);
        esp_wifi_connect();
        s_status.started = true;
        return;
    }

    if (event_id == WIFI_EVENT_STA_CONNECTED) {
        ESP_LOGI(TAG, "connected to access point");
        return;
    }

    if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
        const wifi_event_sta_disconnected_t *disc =
            (const wifi_event_sta_disconnected_t *)event_data;

        s_status.connected = false;
        s_status.ip_addr[0] = '\0';
        s_status.retry_count++;

        ESP_LOGW(
            TAG,
            "Wi-Fi disconnected, reason=%d retry=%" PRIu32 "/%d",
            disc ? disc->reason : -1,
            s_status.retry_count,
            EDGEGUARD_WIFI_MAXIMUM_RETRY
        );

        if (EDGEGUARD_WIFI_MAXIMUM_RETRY == 0 ||
            s_status.retry_count < EDGEGUARD_WIFI_MAXIMUM_RETRY) {
            esp_wifi_connect();
        } else {
            ESP_LOGE(TAG, "maximum retry limit reached, not reconnecting automatically");
        }
    }
}

static void ip_event_handler(
    void *arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void *event_data)
{
    (void)arg;
    (void)event_base;

    if (event_id != IP_EVENT_STA_GOT_IP || event_data == NULL) {
        return;
    }

    const ip_event_got_ip_t *event = (const ip_event_got_ip_t *)event_data;

    snprintf(
        s_status.ip_addr,
        sizeof(s_status.ip_addr),
        IPSTR,
        IP2STR(&event->ip_info.ip)
    );

    s_status.connected = true;
    s_status.retry_count = 0;

    ESP_LOGI(TAG, "got ip: %s", s_status.ip_addr);
}

esp_err_t network_manager_init(void)
{
    if (s_status.initialized) {
        ESP_LOGW(TAG, "already initialized");
        return ESP_OK;
    }

    if (credentials_look_unconfigured()) {
        ESP_LOGW(
            TAG,
            "Wi-Fi credentials are not configured. Update EDGEGUARD_WIFI_SSID / EDGEGUARD_WIFI_PASSWORD in app_config.h"
        );
        s_status.initialized = true;
        return ESP_OK;
    }

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_RETURN_ON_ERROR(nvs_flash_erase(), TAG, "nvs_flash_erase failed");
        ret = nvs_flash_init();
    }
    ESP_RETURN_ON_ERROR(ret, TAG, "nvs_flash_init failed");

    ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "esp_netif_init failed");
    ESP_RETURN_ON_ERROR(esp_event_loop_create_default(), TAG, "esp_event_loop_create_default failed");

    s_sta_netif = esp_netif_create_default_wifi_sta();
    ESP_RETURN_ON_FALSE(s_sta_netif != NULL, ESP_FAIL, TAG, "failed to create default wifi sta");

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&cfg), TAG, "esp_wifi_init failed");

    ESP_RETURN_ON_ERROR(
        esp_event_handler_instance_register(
            WIFI_EVENT,
            ESP_EVENT_ANY_ID,
            &wifi_event_handler,
            NULL,
            &s_any_wifi_handler),
        TAG,
        "register wifi event handler failed"
    );

    ESP_RETURN_ON_ERROR(
        esp_event_handler_instance_register(
            IP_EVENT,
            IP_EVENT_STA_GOT_IP,
            &ip_event_handler,
            NULL,
            &s_got_ip_handler),
        TAG,
        "register got ip handler failed"
    );

    wifi_config_t wifi_config = {0};
    strlcpy((char *)wifi_config.sta.ssid, EDGEGUARD_WIFI_SSID, sizeof(wifi_config.sta.ssid));
    strlcpy((char *)wifi_config.sta.password, EDGEGUARD_WIFI_PASSWORD, sizeof(wifi_config.sta.password));

    wifi_config.sta.threshold.authmode =
        (strlen(EDGEGUARD_WIFI_PASSWORD) == 0) ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_PSK;
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = false;

    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "esp_wifi_set_mode failed");
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_STA, &wifi_config), TAG, "esp_wifi_set_config failed");
    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "esp_wifi_start failed");

    strlcpy(s_status.ssid, EDGEGUARD_WIFI_SSID, sizeof(s_status.ssid));
    s_status.initialized = true;

    ESP_LOGI(TAG, "Wi-Fi init complete");
    return ESP_OK;
}

bool network_manager_is_connected(void)
{
    return s_status.connected;
}

bool network_manager_credentials_configured(void)
{
    return !credentials_look_unconfigured();
}

const network_manager_status_t *network_manager_get_status(void)
{
    return &s_status;
}

const char *network_manager_get_ip_addr(void)
{
    return s_status.ip_addr;
}