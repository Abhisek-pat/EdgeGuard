
## `docs/milestones.md`

```markdown
# EdgeGuard Milestones

This document tracks build progress for EdgeGuard from bring-up to portfolio-ready demo.

---

## Milestone 1 — Project Bring-Up

### Goal
Create the base ESP-IDF project and verify hardware setup.

### Deliverables
- ESP-IDF project builds successfully
- ESP32-CAM boots and prints serial logs
- LED test output works
- camera initialization succeeds

### Acceptance Criteria
- board flashes without errors
- serial monitor shows stable startup
- no repeated crash/reboot loop
- camera probe succeeds

### Status
- [ ] Not started
- [ ] In progress
- [ ] Done

---

## Milestone 2 — Wi-Fi and Local Dashboard

### Goal
Bring the device online and expose health information through HTTP.

### Deliverables
- Wi-Fi connection established
- IP address shown in serial logs
- local HTTP server runs
- `/api/status` returns valid JSON
- dashboard page loads on local network

### Acceptance Criteria
- device reconnects after Wi-Fi drop
- status page reflects uptime and memory
- dashboard loads from browser reliably

### Status
- [ ] Not started
- [ ] In progress
- [ ] Done

---

## Milestone 3 — MQTT Telemetry

### Goal
Enable event and health publishing to an MQTT broker.

### Deliverables
- MQTT client connects successfully
- health messages publish periodically
- test alert event publishes correctly
- broker topics are documented

### Acceptance Criteria
- reconnect works after broker outage
- payload format remains consistent
- subscriber receives messages reliably

### Status
- [ ] Not started
- [ ] In progress
- [ ] Done

---

## Milestone 4 — Inference Integration

### Goal
Run lightweight on-device detection from live camera frames.

### Deliverables
- frame preprocessing pipeline
- inference output with confidence score
- configurable threshold
- serial inference debug logs

### Acceptance Criteria
- model/detection pipeline runs without memory crashes
- inference result updates consistently
- latency is acceptable for demo use

### Status
- [ ] Not started
- [ ] In progress
- [ ] Done

---

## Milestone 5 — Event Engine

### Goal
Make detection behavior reliable and demo-friendly.

### Deliverables
- thresholding logic
- K-out-of-N smoothing
- cooldown timer
- duplicate suppression
- LED/buzzer alert action

### Acceptance Criteria
- no rapid repeated alert spam
- false positives reduced during static scenes
- event count increments correctly

### Status
- [ ] Not started
- [ ] In progress
- [ ] Done

---

## Milestone 6 — End-to-End Demo

### Goal
Demonstrate the full working system.

### Deliverables
- person enters frame
- alert output triggers
- MQTT event is published
- dashboard updates with latest event
- demo video recorded

### Acceptance Criteria
- end-to-end behavior is repeatable
- demo is stable for multiple runs
- logs and visuals are clean enough for GitHub/LinkedIn

### Status
- [ ] Not started
- [ ] In progress
- [ ] Done

---

## Milestone 7 — Portfolio Polish

### Goal
Make the project ready for CV, GitHub, and LinkedIn.

### Deliverables
- architecture diagram
- cleaned-up README
- photos/screenshots
- benchmark notes
- lessons learned section
- final repo structure

### Acceptance Criteria
- repository is understandable to recruiters and engineers
- README tells a clear technical story
- media assets are present
- project looks complete, not abandoned

### Status
- [ ] Not started
- [x] In progress
- [ ] Done

---

## Stretch Goals

- [ ] mDNS support
- [ ] OTA update flow
- [ ] cloud dashboard integration
- [ ] custom-trained quantized model
- [ ] snapshot storage
- [ ] low-power mode

---

## Suggested Weekly Execution Plan

### Week 1
Bring-up, serial logging, camera init

### Week 2
Wi-Fi, HTTP dashboard, basic status APIs

### Week 3
MQTT publish, topic design, reconnect logic

### Week 4
Inference integration and threshold tuning

### Week 5
Event smoothing, cooldown logic, demo stability

### Week 6
Documentation, visuals, final showcase video

## Current Status

EdgeGuard currently supports camera initialization, Wi-Fi connectivity, a local HTTP dashboard, MQTT health/event publishing, LED alert triggering, and a test event pipeline with cooldown handling. The next phase is integrating real on-device inference from live camera frames.