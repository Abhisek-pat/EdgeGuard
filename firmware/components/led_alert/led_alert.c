#include "led_alert.h"

#include "app_config.h"
#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "led_alert";
static bool s_initialized = false;

esp_err_t led_alert_set(bool on)
{
    ESP_RETURN_ON_FALSE(s_initialized, ESP_ERR_INVALID_STATE, TAG, "led_alert not initialized");
    ESP_RETURN_ON_ERROR(gpio_set_level(EDGEGUARD_ALERT_LED_GPIO, on ? 1 : 0), TAG, "gpio_set_level failed");
    return ESP_OK;
}

esp_err_t led_alert_blink(uint32_t on_ms, uint32_t off_ms, uint32_t count)
{
    ESP_RETURN_ON_FALSE(s_initialized, ESP_ERR_INVALID_STATE, TAG, "led_alert not initialized");

    for (uint32_t i = 0; i < count; i++) {
        ESP_RETURN_ON_ERROR(led_alert_set(true), TAG, "failed to turn led on");
        vTaskDelay(pdMS_TO_TICKS(on_ms));

        ESP_RETURN_ON_ERROR(led_alert_set(false), TAG, "failed to turn led off");
        if (i + 1 < count) {
            vTaskDelay(pdMS_TO_TICKS(off_ms));
        }
    }

    return ESP_OK;
}

esp_err_t led_alert_init(void)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "already initialized");
        return ESP_OK;
    }

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << EDGEGUARD_ALERT_LED_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    ESP_RETURN_ON_ERROR(gpio_config(&io_conf), TAG, "gpio_config failed");
    ESP_RETURN_ON_ERROR(gpio_set_level(EDGEGUARD_ALERT_LED_GPIO, 0), TAG, "gpio_set_level failed");

    s_initialized = true;
    ESP_LOGI(TAG, "initialized on GPIO %d", EDGEGUARD_ALERT_LED_GPIO);
    return ESP_OK;
}