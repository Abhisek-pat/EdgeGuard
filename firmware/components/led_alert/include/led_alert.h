#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t led_alert_init(void);
esp_err_t led_alert_blink(uint32_t on_ms, uint32_t off_ms, uint32_t count);
esp_err_t led_alert_set(bool on);

#ifdef __cplusplus
}
#endif