#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    bool initialized;
    bool started;
    bool connected;
    uint32_t retry_count;
    char ssid[33];
    char ip_addr[16];
} network_manager_status_t;

esp_err_t network_manager_init(void);

bool network_manager_is_connected(void);
bool network_manager_credentials_configured(void);
const network_manager_status_t *network_manager_get_status(void);
const char *network_manager_get_ip_addr(void);

#ifdef __cplusplus
}
#endif