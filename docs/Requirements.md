Functional requirements
# FR1 — Camera capture

The system shall capture frames from the ESP32-CAM sensor at a reduced resolution suitable for embedded inference.

# FR2 — Edge inference

The system shall run an on-device detection/classification pipeline to determine whether a person is present in the frame.

# FR3 — Alert actuation

The system shall trigger a local alert using LED or buzzer when a detection event is confirmed.

# FR4 — Event publishing

The system shall publish an MQTT message containing event metadata:

timestamp
confidence
event type
device ID
uptime
RSSI

# FR5 — Local monitoring

The system shall expose an HTTP dashboard showing:

system status
Wi-Fi info
detection count
last detection timestamp
current threshold
health metrics

# FR6 — Reliability

The system shall recover from Wi-Fi disconnection and continue operating without reboot loops.

# Non-functional requirements
Low memory footprint
Near real-time response
Fault tolerance
Readable, modular firmware
Portable documentation
CV-worthy engineering clarity