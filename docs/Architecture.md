# High Level Architecture

+----------------------+
|      ESP32-CAM       |
|----------------------|
| Camera Driver        |
| Inference Engine     |
| Event Logic          |
| HTTP Server          |
| MQTT Client          |
| FreeRTOS Tasks       |
+----------+-----------+
           |
           | Wi-Fi
           v
+----------------------+        +----------------------+
| Local Web Dashboard  |        | MQTT Broker / Cloud  |
| Status / Health      |        | Event Telemetry      |
| Last Detection       |        | Alerts / Dashboard   |
| Event Count          |        | Storage / Analytics  |
+----------------------+        +----------------------+
           |
           v
+----------------------+
| Local Actuation      |
| LED / Buzzer         |
+----------------------+




Embedded system architecture
Main firmware modules
# 1. Camera module

Responsible for:

camera initialization
frame acquisition
frame resizing / preprocessing
handing frames to inference task
# 2. Inference module

Responsible for:

loading the embedded model
input normalization
running inference
confidence thresholding
temporal smoothing
# 3. Event manager

Responsible for:

debouncing repeated detections
managing cooldown intervals
incrementing counters
dispatching LED / buzzer / MQTT actions
# 4. Network manager

Responsible for:

Wi-Fi connection
reconnect handling
mDNS
time sync if used
# 5. MQTT client

Responsible for:

broker connection
topic publishing
optional subscription for remote config
# 6. Web server

Responsible for:

device status endpoint
dashboard HTML
JSON API for metrics
# 7. Storage / config

Responsible for:

device settings
saved threshold values
boot counters
optional event persistence

# 9. FreeRTOS task design

A strong design is:

Task A — Camera Task
captures frame every N ms
sends frame pointer / metadata to queue
Task B — Inference Task
receives frame from queue
preprocesses input
runs inference
sends result to event queue
Task C — Event Task
reads inference results
applies cooldown / smoothing
triggers LED / buzzer
updates counters
publishes event
Task D — Network Task
monitors Wi-Fi and MQTT connection state
reconnects if needed
Task E — Web Task
serves dashboard and JSON status endpoint
Synchronization primitives
queue for frame passing
queue for inference results
mutex for shared state
event group for Wi-Fi/MQTT readiness

# AI / detection approach

Since ESP32-CAM is resource-constrained, structure the AI story in phases.

Option A — Practical MVP

Use existing lightweight face/person detection approach supported by ESP32 ecosystem.

Best for:

getting the demo working fast
proving end-to-end system design
Option B — Custom TinyML model

Train a small classifier such as:

person_present
no_person

# Pipeline:

collect small image dataset
resize to low resolution
train in Colab
quantize to INT8
convert to C array
embed in firmware

Event logic

To avoid noisy alerts:

Suggested logic
require confidence > threshold
require K positive frames out of last N frames
apply cooldown after event
do not publish duplicate events continuously
Example
frame interval: 500 ms
threshold: 0.75
positive count required: 2 of last 3 frames
cooldown: 10 seconds

This makes the system look more production-minded.

# Dashboard should show
project name: EdgeGuard
device ID
Wi-Fi status
MQTT status
free heap
uptime
event counter
last detection time
threshold setting
button for test alert
optional latest snapshot

# Firmware
ESP-IDF
FreeRTOS
esp32-camera
esp_http_server
MQTT client
NVS for config
mDNS
Training
Google Colab
TensorFlow / TFLite
Python preprocessing pipeline
IoT backend

Pick one:

MQTT broker + Node-RED
Mosquitto + dashboard
AWS IoT Core
Home Assistant
Repo tooling
CMake
Python helper scripts
Markdown docs
diagrams in docs/