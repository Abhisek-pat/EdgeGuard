#include "storage.h"

#include "esp_log.h"

static const char *TAG = "storage";

esp_err_t storage_init(void)
{
    ESP_LOGI(TAG, "initialized (scaffold)");
    return ESP_OK;
}
