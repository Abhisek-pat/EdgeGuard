# EdgeGuard

EdgeGuard is an **ESP32-CAM based edge AI security node** that performs lightweight **on-device human presence / intrusion detection**, triggers a local alert, and publishes event telemetry over Wi-Fi using MQTT.

The project is designed to demonstrate practical skills across **edge AI, embedded systems, FreeRTOS, and IoT connectivity** on a resource-constrained device.

---

## Features

- On-device camera-based detection on ESP32-CAM
- Local LED/buzzer alert on intrusion event
- MQTT event publishing to local or cloud broker
- HTTP dashboard for live device health and status
- FreeRTOS task-based firmware architecture
- Event cooldown and duplicate suppression logic
- Configurable thresholds for inference decisions

---

## Why this project

Many low-cost camera systems rely on continuous video streaming or cloud-side inference. EdgeGuard pushes decision-making to the device itself, reducing latency, bandwidth usage, and privacy risk.

This project focuses on building a **real embedded edge vision pipeline**, not just a camera stream demo.

---

## System Architecture

```text
Camera Capture -> Frame Preprocessing -> On-Device Inference -> Event Logic
                                                      |-> LED/Buzzer Alert
                                                      |-> MQTT Publish
                                                      |-> Dashboard Update


# Build Commands
idf.py set-target esp32
idf.py build
idf.py flash
idf.py monitor

 & "C:\Program Files\mosquitto\mosquitto.exe" -c "C:\Program Files\Mosquitto\edgeguard.conf.txt" -v

 & "C:\Program Files\mosquitto\mosquitto_sub.exe" -h 192.168.1.71 -t "edgeguard/device/#" -v

 # change the below lines as per your configuration
 #define EDGEGUARD_WIFI_SSID             "xxxxx_2.4G"
#define EDGEGUARD_WIFI_PASSWORD         "xxxxxxxxxx"

python tools/capture_dataset.py --base-url http://192.168.1.207 --label empty --count 200 --delay 1.2

python tools/capture_dataset.py --base-url http://192.168.1.207 --label person --count 200 --delay 1.2