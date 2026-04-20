#include "inference_engine.h"

#include "esp_log.h"

static const char *TAG = "inference_engine";

esp_err_t inference_engine_init(void)
{
    ESP_LOGI(TAG, "initialized (scaffold)");
    return ESP_OK;
}
