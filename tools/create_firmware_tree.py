from __future__ import annotations

from pathlib import Path
import textwrap


ROOT = Path(".")
FIRMWARE = ROOT / "firmware"
OVERWRITE = True


def write_file(path: Path, content: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.exists() and not OVERWRITE:
        return
    path.write_text(textwrap.dedent(content).lstrip(), encoding="utf-8")
    print(f"wrote: {path}")


def component_files(name: str, func_name: str, header_guard: str, extra_decl: str = "") -> tuple[str, str, str]:
    cmake = f"""
    idf_component_register(
        SRCS "{name}.c"
        INCLUDE_DIRS "include"
        REQUIRES log
    )
    """

    header = f"""
    #pragma once

    #include "esp_err.h"

    esp_err_t {func_name}(void);
    {extra_decl}
    """

    source = f"""
    #include "{name}.h"

    #include "esp_log.h"

    static const char *TAG = "{name}";

    esp_err_t {func_name}(void)
    {{
        ESP_LOGI(TAG, "initialized (scaffold)");
        return ESP_OK;
    }}
    """

    return cmake, header, source


def main() -> None:
    # Root firmware files
    write_file(
        FIRMWARE / "CMakeLists.txt",
        """
        cmake_minimum_required(VERSION 3.16)

        include($ENV{IDF_PATH}/tools/cmake/project.cmake)
        project(edgeguard)
        """,
    )

    write_file(
        FIRMWARE / "sdkconfig.defaults",
        """
        CONFIG_PARTITION_TABLE_CUSTOM=y
        CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.csv"
        CONFIG_ESP_MAIN_TASK_STACK_SIZE=8192
        CONFIG_LOG_DEFAULT_LEVEL_INFO=y
        """,
    )

    write_file(
        FIRMWARE / "partitions.csv",
        """
        # Name,     Type, SubType, Offset,   Size,      Flags
        nvs,        data, nvs,     0x9000,   0x6000,
        otadata,    data, ota,     0xf000,   0x2000,
        phy_init,   data, phy,     0x11000,  0x1000,
        factory,    app,  factory, 0x20000,  0x180000,
        storage,    data, spiffs,  0x1A0000, 0x260000,
        """,
    )

    # main/
    write_file(
        FIRMWARE / "main" / "CMakeLists.txt",
        """
        idf_component_register(
            SRCS
                "main.c"
                "app_state.c"
            INCLUDE_DIRS
                "."
            REQUIRES
                freertos
                log
                esp_system
                camera_service
                inference_engine
                event_manager
                network_manager
                mqtt_service
                web_server
                led_alert
                storage
        )
        """,
    )

    write_file(
        FIRMWARE / "main" / "app_config.h",
        """
        #pragma once

        #define EDGEGUARD_PROJECT_NAME           "EdgeGuard"
        #define EDGEGUARD_FW_VERSION             "0.1.0"
        #define EDGEGUARD_DEVICE_ID              "esp32cam-01"

        #define EDGEGUARD_CAMERA_PERIOD_MS       500
        #define EDGEGUARD_HEARTBEAT_PERIOD_MS    5000
        #define EDGEGUARD_EVENT_COOLDOWN_MS      10000

        #define EDGEGUARD_FRAME_QUEUE_LEN        2
        #define EDGEGUARD_RESULT_QUEUE_LEN       4

        #define EDGEGUARD_DEFAULT_THRESHOLD      0.75f
        """,
    )

    write_file(
        FIRMWARE / "main" / "app_state.h",
        """
        #pragma once

        #include <stdbool.h>
        #include <stdint.h>

        #include "freertos/FreeRTOS.h"
        #include "freertos/semphr.h"

        typedef struct
        {
            bool wifi_connected;
            bool mqtt_connected;
            bool alert_active;

            uint32_t boot_count;
            uint32_t event_count;
            uint32_t uptime_sec;
            uint32_t last_event_ms;

            float threshold;
            float last_confidence;

            SemaphoreHandle_t mutex;
        } edgeguard_state_t;

        void app_state_init(void);
        edgeguard_state_t *app_state_get(void);

        void app_state_set_wifi_connected(bool value);
        void app_state_set_mqtt_connected(bool value);
        void app_state_set_alert_active(bool value);

        void app_state_set_threshold(float value);
        void app_state_set_last_confidence(float value);
        void app_state_increment_event_count(void);
        void app_state_set_last_event_ms(uint32_t value);
        void app_state_set_uptime_sec(uint32_t value);
        """,
    )

    write_file(
        FIRMWARE / "main" / "app_state.c",
        """
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
        """,
    )

    write_file(
        FIRMWARE / "main" / "main.c",
        """
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

        static const char *TAG = "edgeguard_main";

        static void heartbeat_task(void *arg)
        {
            (void)arg;

            while (1) {
                edgeguard_state_t *state = app_state_get();
                app_state_set_uptime_sec((uint32_t)(xTaskGetTickCount() / configTICK_RATE_HZ));

                ESP_LOGI(
                    TAG,
                    "heartbeat | wifi=%d mqtt=%d alert=%d events=%" PRIu32 " threshold=%.2f last_conf=%.2f uptime=%" PRIu32 "s",
                    state->wifi_connected,
                    state->mqtt_connected,
                    state->alert_active,
                    state->event_count,
                    state->threshold,
                    state->last_confidence,
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
            ESP_RETURN_ON_ERROR(inference_engine_init(), TAG, "inference_engine init failed");
            ESP_RETURN_ON_ERROR(event_manager_init(), TAG, "event_manager init failed");
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
        """,
    )

    # Components
    components = [
        ("camera_service", "camera_service_init"),
        ("inference_engine", "inference_engine_init"),
        ("event_manager", "event_manager_init"),
        ("network_manager", "network_manager_init"),
        ("mqtt_service", "mqtt_service_init"),
        ("web_server", "web_server_init"),
        ("led_alert", "led_alert_init"),
        ("storage", "storage_init"),
    ]

    for comp_name, init_fn in components:
        cmake, header, source = component_files(
            name=comp_name,
            func_name=init_fn,
            header_guard=comp_name.upper(),
        )

        write_file(FIRMWARE / "components" / comp_name / "CMakeLists.txt", cmake)
        write_file(FIRMWARE / "components" / comp_name / "include" / f"{comp_name}.h", header)
        write_file(FIRMWARE / "components" / comp_name / f"{comp_name}.c", source)

    print("\\nDone. Firmware scaffold created under ./firmware")


if __name__ == "__main__":
    main()