# EdgeGuard Bill of Materials

This document lists the hardware required for building EdgeGuard.

---

## Core Components

### 1. ESP32-CAM
Primary microcontroller and camera module used for image capture, inference, and network connectivity.

**Notes**
- typically based on ESP32 with OV2640 camera
- limited RAM, so design must stay lightweight
- some boards are unstable on weak USB power

### 2. USB-to-TTL Programmer
Required for flashing firmware onto the ESP32-CAM.

**Notes**
- common options: FTDI, CP2102, CH340 based adapters
- must support 3.3V logic
- needed for initial flashing and debugging

### 3. External Power Source
Recommended for stable operation.

**Notes**
- ESP32-CAM can brown out during camera + Wi-Fi activity
- use a stable 5V supply where possible

---

## Alert Components

### 4. LED
Used for simple visual alert indication.

**Recommended**
- 1 x LED
- 1 x current-limiting resistor (220 ohm to 330 ohm)

### 5. Buzzer (Optional)
Used for audible event alerts.

**Notes**
- optional, but useful for demos
- active buzzer is simpler than passive buzzer

---

## Optional Expansion Components

### 6. Breadboard and Jumper Wires
Useful for prototyping LED/buzzer alert circuits.

### 7. MicroSD Card
Optional if event snapshots or local logging are added.

### 8. Enclosure
Optional for final demo polish and mounting.

### 9. Push Button
Optional for reset, test alert, or configuration mode.

---

## Suggested Minimum Setup

For the MVP version of EdgeGuard, you only need:

- ESP32-CAM
- USB-to-TTL programmer
- stable power source
- LED and resistor
- jumper wires

---

## Estimated Roles of Each Component

| Component | Purpose |
|---|---|
| ESP32-CAM | Vision capture, inference, networking |
| USB-to-TTL | Flashing and serial logs |
| LED | Visual alert |
| Buzzer | Audible alert |
| Power supply | Stable runtime behavior |
| Jumper wires | Connections during prototype phase |

---

## Wiring Notes

### LED Alert
- connect LED anode through resistor to chosen GPIO
- connect cathode to GND

### Buzzer Alert
- connect to selected GPIO and GND
- use transistor driver if buzzer current is too high

### Programming
Typical flashing setup requires:
- `U0R` / `TX`
- `U0T` / `RX`
- `5V`
- `GND`
- `GPIO0` pulled low during flashing

---

## Power and Stability Notes

ESP32-CAM boards are often sensitive to power quality.

Best practices:
- avoid weak USB power
- keep wiring short
- test boot stability before adding extra peripherals
- if random resets occur, check supply voltage first

---

## Future Hardware Enhancements

- PIR sensor for hybrid motion + vision trigger
- better antenna / Wi-Fi optimization
- relay output for external alarm integration
- battery pack for portable deployment
- enclosure for wall/door mounting