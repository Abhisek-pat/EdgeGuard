#include "web_server.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app_config.h"
#include "app_state.h"
#include "camera_service.h"
#include "esp_check.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "event_manager.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "inference_engine.h"
#include "mqtt_service.h"
#include "network_manager.h"

static const char *TAG = "web_server";

static httpd_handle_t s_server = NULL;
static TaskHandle_t s_web_task_handle = NULL;

static esp_err_t capture_jpg_get_handler(httpd_req_t *req);

static const char *INDEX_HTML =
"<!doctype html>"
"<html lang='en'>"
"<head>"
"<meta charset='utf-8'>"
"<meta name='viewport' content='width=device-width,initial-scale=1'>"
"<title>EdgeGuard Console</title>"
"<style>"
":root{"
"  --bg:#07111c;"
"  --bg2:#0b1624;"
"  --panel:#101b2a;"
"  --panel2:#142235;"
"  --panel3:#0d1725;"
"  --border:rgba(255,255,255,.07);"
"  --text:#edf4ff;"
"  --muted:#8aa0b8;"
"  --muted2:#6f8398;"
"  --ok:#1ec97c;"
"  --warn:#f5b644;"
"  --bad:#ff6767;"
"  --info:#5aa9ff;"
"  --accent:#58a6ff;"
"  --accent2:#78d2ff;"
"  --glow:0 14px 36px rgba(0,0,0,.28);"
"}"
"*{box-sizing:border-box}"
"html,body{margin:0;padding:0;background:linear-gradient(180deg,var(--bg) 0%, var(--bg2) 100%);color:var(--text);font-family:Inter,Segoe UI,Arial,sans-serif}"
"body{padding:22px}"
".wrap{max-width:1480px;margin:0 auto}"
".header{display:flex;justify-content:space-between;gap:18px;align-items:flex-start;flex-wrap:wrap;margin-bottom:16px}"
".title h1{margin:0;font-size:36px;line-height:1.02;font-weight:800;letter-spacing:-.02em}"
".title p{margin:8px 0 0;color:var(--muted);font-size:15px}"
".meta-row,.chips{display:flex;gap:10px;flex-wrap:wrap}"
".meta-row{margin-top:14px}"
".chip{display:inline-flex;align-items:center;gap:8px;padding:8px 12px;border-radius:999px;background:rgba(255,255,255,.035);border:1px solid var(--border);color:var(--muted);font-size:12px}"
".dot{width:8px;height:8px;border-radius:50%}"
".ok .dot{background:var(--ok)}"
".warn .dot{background:var(--warn)}"
".bad .dot{background:var(--bad)}"
".info .dot{background:var(--info)}"
".banner{display:flex;align-items:center;justify-content:space-between;gap:14px;flex-wrap:wrap;padding:14px 16px;margin-bottom:16px;border-radius:18px;background:linear-gradient(180deg,rgba(88,166,255,.10),rgba(88,166,255,.04));border:1px solid rgba(88,166,255,.18);box-shadow:var(--glow)}"
".banner-left{display:flex;align-items:center;gap:12px;flex-wrap:wrap}"
".pulse{width:12px;height:12px;border-radius:50%;background:var(--info);box-shadow:0 0 0 0 rgba(90,169,255,.55);animation:pulse 2s infinite}"
"@keyframes pulse{0%{box-shadow:0 0 0 0 rgba(90,169,255,.5)}70%{box-shadow:0 0 0 12px rgba(90,169,255,0)}100%{box-shadow:0 0 0 0 rgba(90,169,255,0)}}"
".banner-title{font-size:15px;font-weight:700}"
".banner-sub{font-size:12px;color:var(--muted)}"
".badge{display:inline-flex;align-items:center;padding:6px 10px;border-radius:999px;font-size:11px;font-weight:700;text-transform:uppercase;letter-spacing:.08em}"
".badge.ok{background:rgba(30,201,124,.16);color:#8ff0bf}"
".badge.warn{background:rgba(245,182,68,.14);color:#ffd37f}"
".badge.bad{background:rgba(255,103,103,.14);color:#ffadad}"
".badge.info{background:rgba(90,169,255,.14);color:#9dc8ff}"
".grid{display:grid;gap:14px}"
".overview{grid-template-columns:repeat(6,minmax(0,1fr));margin-bottom:14px}"
".layout{display:grid;grid-template-columns:1.18fr .82fr;gap:14px;margin-bottom:14px}"
".card{background:linear-gradient(180deg,var(--panel) 0%,var(--panel2) 100%);border:1px solid var(--border);border-radius:20px;padding:18px;box-shadow:var(--glow)}"
".card-flat{background:linear-gradient(180deg,var(--panel3) 0%,var(--panel2) 100%)}"
".label{font-size:11px;letter-spacing:.16em;text-transform:uppercase;color:var(--muted);margin-bottom:8px}"
".value{font-size:16px;font-weight:700;line-height:1.25}"
".value.big{font-size:28px;line-height:1.05}"
".sub{margin-top:8px;color:var(--muted);font-size:12px;line-height:1.4}"
".mono{font-family:ui-monospace,SFMono-Regular,Menlo,Consolas,monospace}"
".section-title{display:flex;justify-content:space-between;align-items:center;gap:10px;margin-bottom:14px;flex-wrap:wrap}"
".section-title h2{margin:0;font-size:19px}"
".section-title span{font-size:12px;color:var(--muted)}"

".hero{display:grid;grid-template-columns:240px 1fr;gap:18px;align-items:center}"
".score-orb{position:relative;width:210px;height:210px;border-radius:50%;margin:0 auto;background:radial-gradient(circle at 50% 50%,rgba(88,166,255,.24),rgba(88,166,255,.06));border:1px solid rgba(255,255,255,.08);display:flex;align-items:center;justify-content:center}"
".score-orb::after{content:'';position:absolute;inset:16px;border-radius:50%;border:1px solid rgba(255,255,255,.05)}"
".score-core{text-align:center;position:relative;z-index:2}"
".score-core .n{font-size:42px;font-weight:800;line-height:1}"
".score-core .t{margin-top:10px;font-size:11px;color:var(--muted);text-transform:uppercase;letter-spacing:.16em}"
".metric-grid{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:12px}"
".metric{padding:14px;border-radius:16px;background:rgba(255,255,255,.03);border:1px solid var(--border)}"
".metric .k{font-size:11px;color:var(--muted);text-transform:uppercase;letter-spacing:.14em}"
".metric .v{margin-top:8px;font-size:17px;font-weight:700;line-height:1.3}"
".bar-wrap{margin-top:16px}"
".bar-shell{position:relative;height:18px;border-radius:999px;background:#0b1420;border:1px solid var(--border);overflow:hidden}"
".bar-fill{position:absolute;left:0;top:0;height:100%;width:0%;background:linear-gradient(90deg,var(--accent),var(--accent2))}"
".bar-threshold{position:absolute;top:-2px;bottom:-2px;width:2px;background:var(--warn)}"
".bar-reset{position:absolute;top:2px;bottom:2px;width:2px;background:var(--ok)}"
".legend{display:flex;gap:16px;flex-wrap:wrap;margin-top:10px;color:var(--muted);font-size:12px}"
".legend i{display:inline-block;width:10px;height:10px;border-radius:3px;margin-right:6px;vertical-align:middle}"
".spark-wrap{margin-top:14px;padding:10px 12px;border-radius:14px;background:rgba(255,255,255,.025);border:1px solid var(--border)}"
".spark-title{display:flex;justify-content:space-between;align-items:center;gap:8px;margin-bottom:6px}"
".spark-title span{font-size:12px;color:var(--muted)}"
"#scoreSparkline{width:100%;height:92px;display:block}"

".health-grid{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:12px}"
".table-wrap{overflow:auto;border-radius:14px;border:1px solid var(--border);background:rgba(0,0,0,.10)}"
"table{width:100%;border-collapse:collapse;min-width:700px}"
"th,td{padding:12px 14px;text-align:left;font-size:13px}"
"th{background:rgba(255,255,255,.03);color:var(--muted);font-weight:600;text-transform:uppercase;letter-spacing:.08em}"
"tr+tr td{border-top:1px solid rgba(255,255,255,.05)}"
".actions{display:flex;gap:10px;flex-wrap:wrap}"
"button{appearance:none;border:none;border-radius:12px;padding:12px 16px;background:linear-gradient(180deg,#2d8cff,#1975f7);color:#fff;font-weight:700;cursor:pointer;box-shadow:0 12px 22px rgba(25,117,247,.26)}"
"button.secondary{background:linear-gradient(180deg,#223247,#1a2738);color:var(--text)}"
"button:disabled{opacity:.65;cursor:not-allowed}"
".footer-note{margin-top:10px;color:var(--muted);font-size:12px;line-height:1.45}"
".stack{display:grid;gap:12px}"
".tiny{font-size:12px;color:var(--muted)}"
".muted{color:var(--muted)}"
".right{display:flex;align-items:center;gap:8px;flex-wrap:wrap}"
".status-block{display:flex;gap:10px;align-items:center;flex-wrap:wrap}"
".empty-state{padding:26px 16px;text-align:center;color:var(--muted)}"

"@media (max-width:1260px){"
"  .overview{grid-template-columns:repeat(3,minmax(0,1fr))}"
"  .layout{grid-template-columns:1fr}"
"  .hero{grid-template-columns:1fr}"
"  .health-grid{grid-template-columns:repeat(2,minmax(0,1fr))}"
"}"
"@media (max-width:760px){"
"  body{padding:14px}"
"  .overview{grid-template-columns:repeat(2,minmax(0,1fr))}"
"  .health-grid{grid-template-columns:1fr}"
"  .metric-grid{grid-template-columns:1fr}"
"  .score-orb{width:180px;height:180px}"
"  .score-core .n{font-size:34px}"
"  .title h1{font-size:30px}"
"}"
"</style>"
"</head>"
"<body>"
"<div class='wrap'>"

"<div class='header'>"
"  <div class='title'>"
"    <h1>EdgeGuard Console</h1>"
"    <p>ESP32-CAM Edge AI monitoring dashboard</p>"
"    <div class='meta-row'>"
"      <div class='chip info'><span class='dot'></span><span id='metaDevice'>Device</span></div>"
"      <div class='chip info'><span class='dot'></span><span id='metaFw'>Firmware</span></div>"
"      <div class='chip info'><span class='dot'></span><span id='metaUpdated'>Updated</span></div>"
"    </div>"
"  </div>"
"  <div class='chips' id='statusChips'>"
"    <div class='chip info'><span class='dot'></span><span>Loading</span></div>"
"  </div>"
"</div>"

"<div class='banner'>"
"  <div class='banner-left'>"
"    <div class='pulse'></div>"
"    <div>"
"      <div class='banner-title' id='operatorTitle'>Waiting for device telemetry...</div>"
"      <div class='banner-sub' id='operatorSub'>Checking connectivity, inference state, and event pipeline.</div>"
"    </div>"
"  </div>"
"  <div class='right'>"
"    <span class='badge info' id='bannerBadge'>Initializing</span>"
"  </div>"
"</div>"

"<div class='grid overview'>"
"  <div class='card card-flat'>"
"    <div class='label'>System Status</div>"
"    <div class='value' id='sysStatusValue'>--</div>"
"    <div class='sub' id='sysStatusSub'>--</div>"
"  </div>"
"  <div class='card card-flat'>"
"    <div class='label'>Wi-Fi</div>"
"    <div class='value' id='wifiValue'>--</div>"
"    <div class='sub' id='wifiSub'>--</div>"
"  </div>"
"  <div class='card card-flat'>"
"    <div class='label'>MQTT Broker</div>"
"    <div class='value' id='mqttValue'>--</div>"
"    <div class='sub mono' id='mqttSub'>--</div>"
"  </div>"
"  <div class='card card-flat'>"
"    <div class='label'>Uptime</div>"
"    <div class='value' id='uptimeValue'>--</div>"
"    <div class='sub'>Continuous runtime</div>"
"  </div>"
"  <div class='card card-flat'>"
"    <div class='label'>Last Event</div>"
"    <div class='value' id='lastEventValue'>--</div>"
"    <div class='sub' id='lastEventSub'>--</div>"
"  </div>"
"  <div class='card card-flat'>"
"    <div class='label'>Free Heap</div>"
"    <div class='value' id='heapValue'>--</div>"
"    <div class='sub'>Available memory on device</div>"
"  </div>"
"</div>"

"<div class='layout'>"
"  <div class='card'>"
"    <div class='section-title'>"
"      <h2>Inference Control</h2>"
"      <span class='badge info' id='inferStateBadge'>Loading</span>"
"    </div>"
"    <div class='hero'>"
"      <div class='score-orb'>"
"        <div class='score-core'>"
"          <div class='n' id='scoreValue'>0.000</div>"
"          <div class='t'>Current Score</div>"
"        </div>"
"      </div>"
"      <div>"
"        <div class='metric-grid'>"
"          <div class='metric'><div class='k'>Decision</div><div class='v' id='decisionValue'>--</div></div>"
"          <div class='metric'><div class='k'>Trigger Latched</div><div class='v' id='latchedValue'>--</div></div>"
"          <div class='metric'><div class='k'>Trigger Threshold</div><div class='v' id='thresholdValue'>--</div></div>"
"          <div class='metric'><div class='k'>Reset Threshold</div><div class='v' id='resetValue'>--</div></div>"
"        </div>"
"        <div class='bar-wrap'>"
"          <div class='bar-shell'>"
"            <div class='bar-fill' id='scoreFill'></div>"
"            <div class='bar-threshold' id='thresholdMarker'></div>"
"            <div class='bar-reset' id='resetMarker'></div>"
"          </div>"
"          <div class='legend'>"
"            <span><i style='background:linear-gradient(90deg,#58a6ff,#78d2ff)'></i>Live score</span>"
"            <span><i style='background:#f5b644'></i>Trigger threshold</span>"
"            <span><i style='background:#1ec97c'></i>Reset threshold</span>"
"          </div>"
"        </div>"
"        <div class='spark-wrap'>"
"          <div class='spark-title'>"
"            <span>Recent Score History</span>"
"            <span id='sparkInfo'>last 40 samples</span>"
"          </div>"
"          <canvas id='scoreSparkline' width='800' height='92'></canvas>"
"        </div>"
"        <div class='footer-note'>This panel shows the live classifier score, decision state, thresholds, and short-term score trend.</div>"
"      </div>"
"    </div>"
"  </div>"

"  <div class='card'>"
"    <div class='section-title'>"
"      <h2>System Health</h2>"
"      <span class='badge info' id='cameraBadge'>Loading</span>"
"    </div>"
"    <div class='health-grid'>"
"      <div class='metric'><div class='k'>Capture Mode</div><div class='v' id='captureModeValue'>--</div></div>"
"      <div class='metric'><div class='k'>Frame</div><div class='v' id='frameValue'>--</div></div>"
"      <div class='metric'><div class='k'>Capture Count</div><div class='v' id='captureCountValue'>--</div></div>"
"      <div class='metric'><div class='k'>Capture Time</div><div class='v' id='captureTimeValue'>--</div></div>"
"      <div class='metric'><div class='k'>Inference Time</div><div class='v' id='inferTimeValue'>--</div></div>"
"      <div class='metric'><div class='k'>Dropped Frames</div><div class='v' id='droppedValue'>--</div></div>"
"    </div>"
"    <div class='footer-note'>Operational metrics from the edge device camera and inference pipeline.</div>"
"  </div>"
"</div>"

"<div class='layout'>"
"  <div class='card'>"
"    <div class='section-title'>"
"      <h2>Recent Events</h2>"
"      <span id='eventSummary'>Loading...</span>"
"    </div>"
"    <div class='table-wrap'>"
"      <table>"
"        <thead>"
"          <tr>"
"            <th>Age</th>"
"            <th>Event</th>"
"            <th>Confidence</th>"
"            <th>Source</th>"
"            <th>Status</th>"
"          </tr>"
"        </thead>"
"        <tbody id='eventsBody'>"
"          <tr><td colspan='5' class='muted'>Loading recent events...</td></tr>"
"        </tbody>"
"      </table>"
"    </div>"
"  </div>"

"  <div class='card'>"
"    <div class='section-title'>"
"      <h2>Operator Actions</h2>"
"      <span>Validation and control</span>"
"    </div>"
"    <div class='actions'>"
"      <button id='btnTest' onclick='triggerTestEvent()'>Trigger Test Event</button>"
"      <button class='secondary' onclick='refreshAll()'>Refresh Now</button>"
"    </div>"
"    <div class='metric-grid' style='margin-top:16px'>"
"      <div class='metric'><div class='k'>Event Count</div><div class='v' id='eventCountValue'>--</div></div>"
"      <div class='metric'><div class='k'>Alert State</div><div class='v' id='alertValue'>--</div></div>"
"      <div class='metric'><div class='k'>Accepted Events</div><div class='v' id='acceptedValue'>--</div></div>"
"      <div class='metric'><div class='k'>Ignored Events</div><div class='v' id='ignoredValue'>--</div></div>"
"    </div>"
"    <div class='footer-note'>Use the test event button to validate event handling, dashboard updates, and MQTT publishing end-to-end.</div>"
"  </div>"
"</div>"

"</div>"

"<script>"
"let latestStatus = null;"
"let scoreHistory = [];"

"function setText(id, value){"
"  const el = document.getElementById(id);"
"  if(el) el.textContent = value;"
"}"

"function fmtUptime(sec){"
"  const s = Number(sec || 0);"
"  const h = Math.floor(s / 3600);"
"  const m = Math.floor((s % 3600) / 60);"
"  const r = s % 60;"
"  if(h > 0) return `${h}h ${m}m ${r}s`;"
"  if(m > 0) return `${m}m ${r}s`;"
"  return `${r}s`;"
"}"

"function fmtAge(nowMs, tsMs){"
"  if(!tsMs) return '—';"
"  const diff = Math.max(0, Math.floor((nowMs - tsMs) / 1000));"
"  if(diff < 60) return `${diff}s ago`;"
"  if(diff < 3600) return `${Math.floor(diff/60)}m ago`;"
"  return `${Math.floor(diff/3600)}h ago`;"
"}"

"function fmtBytes(n){"
"  const v = Number(n || 0);"
"  if(v >= 1024 * 1024) return `${(v / (1024*1024)).toFixed(2)} MB`;"
"  if(v >= 1024) return `${(v / 1024).toFixed(1)} KB`;"
"  return `${v} B`;"
"}"

"function fmtMicro(us){"
"  const v = Number(us || 0);"
"  if(v >= 1000) return `${(v / 1000).toFixed(1)} ms`;"
"  return `${v} us`;"
"}"

"function inferBadgeClass(decision){"
"  if(decision === 'triggered' || decision === 'latched') return 'badge warn';"
"  if(decision === 'monitoring' || decision === 'armed') return 'badge ok';"
"  if(decision === 'warming_up') return 'badge info';"
"  return 'badge info';"
"}"

"function systemSummary(s){"
"  if(!s.wifi_connected) return {title:'Wi-Fi offline', sub:'Device is not connected to the configured access point.', badge:'badge bad'};"
"  if(!s.mqtt_connected) return {title:'Telemetry degraded', sub:'Local dashboard is online but MQTT publishing is disconnected.', badge:'badge warn'};"
"  if(!s.camera_ready) return {title:'Camera fault', sub:'Connectivity is healthy but the camera service is not ready.', badge:'badge bad'};"
"  if(s.alert_active) return {title:'Alert active', sub:'System is actively processing or signaling an alert condition.', badge:'badge warn'};"
"  if(s.inference_decision === 'latched' || s.inference_decision === 'triggered') return {title:'Detection latched', sub:'A recent inference crossed the trigger threshold.', badge:'badge warn'};"
"  return {title:'System healthy', sub:'Connectivity, camera, and inference pipeline are operating normally.', badge:'badge ok'};"
"}"

"function pushScore(score){"
"  scoreHistory.push(Number(score || 0));"
"  if(scoreHistory.length > 40) scoreHistory.shift();"
"}"

"function drawSparkline(threshold, resetThreshold){"
"  const canvas = document.getElementById('scoreSparkline');"
"  if(!canvas) return;"
"  const ctx = canvas.getContext('2d');"
"  const width = canvas.width;"
"  const height = canvas.height;"
"  ctx.clearRect(0,0,width,height);"

"  ctx.strokeStyle = 'rgba(255,255,255,0.08)';"
"  ctx.lineWidth = 1;"
"  for(let i=1;i<4;i++){"
"    const y = (height/4)*i;"
"    ctx.beginPath();"
"    ctx.moveTo(0,y);"
"    ctx.lineTo(width,y);"
"    ctx.stroke();"
"  }"

"  const thY = height - (Math.max(0, Math.min(1, Number(threshold || 0))) * height);"
"  ctx.strokeStyle = '#f5b644';"
"  ctx.lineWidth = 1.5;"
"  ctx.beginPath();"
"  ctx.moveTo(0, thY);"
"  ctx.lineTo(width, thY);"
"  ctx.stroke();"

"  const rsY = height - (Math.max(0, Math.min(1, Number(resetThreshold || 0))) * height);"
"  ctx.strokeStyle = '#1ec97c';"
"  ctx.lineWidth = 1.2;"
"  ctx.beginPath();"
"  ctx.moveTo(0, rsY);"
"  ctx.lineTo(width, rsY);"
"  ctx.stroke();"

"  if(scoreHistory.length < 2){"
"    ctx.fillStyle = 'rgba(138,160,184,.9)';"
"    ctx.font = '12px sans-serif';"
"    ctx.fillText('Waiting for enough samples...', 10, 20);"
"    return;"
"  }"

"  ctx.strokeStyle = '#67b6ff';"
"  ctx.lineWidth = 2;"
"  ctx.beginPath();"
"  scoreHistory.forEach((v, i) => {"
"    const x = (i / (scoreHistory.length - 1)) * width;"
"    const y = height - (Math.max(0, Math.min(1, v)) * height);"
"    if(i === 0) ctx.moveTo(x, y);"
"    else ctx.lineTo(x, y);"
"  });"
"  ctx.stroke();"

"  const last = scoreHistory[scoreHistory.length - 1];"
"  const lx = width;"
"  const ly = height - (Math.max(0, Math.min(1, last)) * height);"
"  ctx.fillStyle = '#67b6ff';"
"  ctx.beginPath();"
"  ctx.arc(lx, ly, 3.5, 0, Math.PI * 2);"
"  ctx.fill();"
"}"

"function renderStatus(s){"
"  latestStatus = s;"
"  pushScore(s.inference_score);"
"  const now = new Date();"

"  setText('metaDevice', `Device: ${s.device_id}`);"
"  setText('metaFw', `FW: ${s.fw_version}`);"
"  setText('metaUpdated', `Updated: ${now.toLocaleTimeString()}`);"

"  const chips = [];"
"  chips.push(`<div class='chip ${s.wifi_connected ? 'ok' : 'bad'}'><span class='dot'></span><span>${s.wifi_connected ? 'Wi-Fi Connected' : 'Wi-Fi Offline'}</span></div>`);"
"  chips.push(`<div class='chip ${s.mqtt_connected ? 'ok' : 'warn'}'><span class='dot'></span><span>${s.mqtt_connected ? 'MQTT Connected' : 'MQTT Reconnecting'}</span></div>`);"
"  chips.push(`<div class='chip ${s.camera_ready ? 'ok' : 'bad'}'><span class='dot'></span><span>${s.camera_ready ? 'Camera Ready' : 'Camera Fault'}</span></div>`);"
"  chips.push(`<div class='chip info'><span class='dot'></span><span>${s.project}</span></div>`);"
"  document.getElementById('statusChips').innerHTML = chips.join('');"

"  const summary = systemSummary(s);"
"  setText('operatorTitle', summary.title);"
"  setText('operatorSub', summary.sub);"
"  const bannerBadge = document.getElementById('bannerBadge');"
"  bannerBadge.className = summary.badge;"
"  bannerBadge.textContent = s.inference_decision || 'unknown';"

"  setText('sysStatusValue', summary.title);"
"  setText('sysStatusSub', summary.sub);"
"  setText('wifiValue', s.wifi_connected ? 'Connected' : 'Disconnected');"
"  setText('wifiSub', s.ssid || 'No SSID');"
"  setText('mqttValue', s.mqtt_connected ? 'Connected' : 'Disconnected');"
"  setText('mqttSub', s.mqtt_broker || 'N/A');"
"  setText('uptimeValue', fmtUptime(s.uptime_sec));"
"  setText('lastEventValue', s.last_event_name || 'none');"
"  setText('lastEventSub', s.last_event_ts_ms ? fmtAge(s.now_ms, s.last_event_ts_ms) : 'No recent event');"
"  setText('heapValue', fmtBytes(s.free_heap_bytes));"

"  setText('scoreValue', Number(s.inference_score || 0).toFixed(3));"
"  setText('decisionValue', s.inference_decision || 'unknown');"
"  setText('latchedValue', s.trigger_latched ? 'Yes' : 'No');"
"  setText('thresholdValue', Number(s.person_threshold || 0).toFixed(2));"
"  setText('resetValue', Number(s.reset_threshold || 0).toFixed(2));"

"  setText('captureModeValue', s.capture_mode || 'unknown');"
"  setText('frameValue', `${s.frame_width}x${s.frame_height} • ${fmtBytes(s.last_frame_len)}`);"
"  setText('captureCountValue', String(s.capture_count));"
"  setText('captureTimeValue', fmtMicro(s.last_capture_time_us));"
"  setText('inferTimeValue', fmtMicro(s.last_inference_time_us));"
"  setText('droppedValue', String(s.dropped_frames));"

"  setText('eventCountValue', String(s.event_count));"
"  setText('alertValue', s.alert_active ? 'Active' : 'Idle');"
"  setText('acceptedValue', String(s.accepted_events));"
"  setText('ignoredValue', String(s.ignored_events));"

"  const scorePct = Math.max(0, Math.min(100, Number(s.inference_score || 0) * 100));"
"  document.getElementById('scoreFill').style.width = `${scorePct}%`;"
"  document.getElementById('thresholdMarker').style.left = `${Math.max(0, Math.min(100, Number(s.person_threshold || 0) * 100))}%`;"
"  document.getElementById('resetMarker').style.left = `${Math.max(0, Math.min(100, Number(s.reset_threshold || 0) * 100))}%`;"

"  const inferBadge = document.getElementById('inferStateBadge');"
"  inferBadge.className = inferBadgeClass(s.inference_decision);"
"  inferBadge.textContent = s.inference_decision || 'unknown';"

"  const camBadge = document.getElementById('cameraBadge');"
"  camBadge.className = `badge ${s.camera_ready ? 'ok' : 'bad'}`;"
"  camBadge.textContent = s.camera_ready ? 'camera ready' : 'camera error';"

"  drawSparkline(s.person_threshold, s.reset_threshold);"
"}"

"function renderEvents(payload){"
"  const body = document.getElementById('eventsBody');"
"  const items = (payload && payload.items) ? payload.items : [];"
"  setText('eventSummary', `${items.length} recent item(s)`);"

"  if(!latestStatus){"
"    body.innerHTML = `<tr><td colspan='5' class='muted'>Waiting for status...</td></tr>`;"
"    return;"
"  }"

"  if(items.length === 0){"
"    body.innerHTML = `<tr><td colspan='5'><div class='empty-state'>No recent events recorded.</div></td></tr>`;"
"    return;"
"  }"

"  body.innerHTML = items.map(item => {"
"    const age = fmtAge(latestStatus.now_ms, item.timestamp_ms);"
"    const src = item.is_test ? 'test' : 'model';"
"    const statusCls = item.accepted ? 'ok' : 'warn';"
"    const statusTxt = item.accepted ? 'accepted' : 'ignored';"
"    return `"
"      <tr>"
"        <td>${age}</td>"
"        <td>${item.name}</td>"
"        <td>${Number(item.confidence || 0).toFixed(2)}</td>"
"        <td><span class='badge info'>${src}</span></td>"
"        <td><span class='badge ${statusCls}'>${statusTxt}</span></td>"
"      </tr>`;"
"  }).join('');"
"}"

"async function fetchJson(path){"
"  const res = await fetch(path, {cache:'no-store'});"
"  if(!res.ok) throw new Error(`${path} -> ${res.status}`);"
"  return await res.json();"
"}"

"async function refreshAll(){"
"  try{"
"    const [status, events] = await Promise.all(["
"      fetchJson('/api/status'),"
"      fetchJson('/api/events')"
"    ]);"
"    renderStatus(status);"
"    renderEvents(events);"
"  }catch(err){"
"    console.error(err);"
"    const body = document.getElementById('eventsBody');"
"    if(body){"
"      body.innerHTML = `<tr><td colspan='5' style='color:#ff6767'>Dashboard refresh failed</td></tr>`;"
"    }"
"    setText('operatorTitle', 'Dashboard refresh failed');"
"    setText('operatorSub', 'The browser could not fetch telemetry from the device.');"
"    const badge = document.getElementById('bannerBadge');"
"    if(badge){"
"      badge.className = 'badge bad';"
"      badge.textContent = 'error';"
"    }"
"  }"
"}"

"async function triggerTestEvent(){"
"  const btn = document.getElementById('btnTest');"
"  btn.disabled = true;"
"  try{"
"    await fetch('/api/test_event', {method:'POST'});"
"    setTimeout(refreshAll, 350);"
"  }catch(err){"
"    console.error(err);"
"  }finally{"
"    setTimeout(() => { btn.disabled = false; }, 700);"
"  }"
"}"

"refreshAll();"
"setInterval(refreshAll, 2500);"
"</script>"

"</body>"
"</html>";

static const char *pixel_format_to_string(pixformat_t fmt)
{
    switch (fmt) {
        case PIXFORMAT_GRAYSCALE: return "grayscale";
        case PIXFORMAT_JPEG: return "jpeg";
        case PIXFORMAT_RGB565: return "rgb565";
        default: return "other";
    }
}

static esp_err_t favicon_get_handler(httpd_req_t *req)
{
    httpd_resp_set_status(req, "204 No Content");
    return httpd_resp_send(req, NULL, 0);
}

static esp_err_t root_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, INDEX_HTML, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t status_get_handler(httpd_req_t *req)
{
    const network_manager_status_t *net = network_manager_get_status();
    const camera_service_status_t *cam = camera_service_get_status();
    const mqtt_service_status_t *mqtt = mqtt_service_get_status();
    const event_manager_status_t *evt = event_manager_get_status();
    const inference_engine_status_t *infer = inference_engine_get_status();
    const edgeguard_state_t *state = app_state_get();

    const uint32_t uptime_sec = (uint32_t)(esp_timer_get_time() / 1000000ULL);
    const uint64_t now_ms = (uint64_t)(esp_timer_get_time() / 1000ULL);
    const uint32_t free_heap = (uint32_t)esp_get_free_heap_size();

    char *response = (char *)malloc(4096);
    if (response == NULL) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "alloc failed");
        return ESP_ERR_NO_MEM;
    }

    snprintf(
        response,
        4096,
        "{"
        "\"project\":\"%s\","
        "\"fw_version\":\"%s\","
        "\"device_id\":\"%s\","
        "\"uptime_sec\":%" PRIu32 ","
        "\"now_ms\":%" PRIu64 ","
        "\"wifi_connected\":%s,"
        "\"ssid\":\"%s\","
        "\"ip_addr\":\"%s\","
        "\"mqtt_connected\":%s,"
        "\"mqtt_broker\":\"%s\","
        "\"camera_ready\":%s,"
        "\"capture_mode\":\"%s\","
        "\"frame_width\":%u,"
        "\"frame_height\":%u,"
        "\"last_frame_len\":%u,"
        "\"last_capture_time_us\":%" PRId64 ","
        "\"capture_count\":%" PRIu32 ","
        "\"inference_score\":%.3f,"
        "\"inference_decision\":\"%s\","
        "\"trigger_latched\":%s,"
        "\"baseline_frame_len\":%u,"
        "\"inference_samples\":%" PRIu32 ","
        "\"inference_triggers\":%" PRIu32 ","
        "\"last_inference_time_us\":%" PRId64 ","
        "\"dropped_frames\":%" PRIu32 ","
        "\"event_count\":%" PRIu32 ","
        "\"last_event_name\":\"%s\","
        "\"last_event_ts_ms\":%" PRIu64 ","
        "\"last_confidence\":%.2f,"
        "\"alert_active\":%s,"
        "\"accepted_events\":%" PRIu32 ","
        "\"ignored_events\":%" PRIu32 ","
        "\"person_threshold\":%.2f,"
        "\"reset_threshold\":%.2f,"
        "\"free_heap_bytes\":%" PRIu32
        "}",
        EDGEGUARD_PROJECT_NAME,
        EDGEGUARD_FW_VERSION,
        EDGEGUARD_DEVICE_ID,
        uptime_sec,
        now_ms,
        net->connected ? "true" : "false",
        net->ssid,
        net->ip_addr,
        mqtt->connected ? "true" : "false",
        mqtt->broker_uri,
        cam->ready ? "true" : "false",
        pixel_format_to_string(cam->pixel_format),
        cam->last_width,
        cam->last_height,
        (unsigned)cam->last_frame_len,
        cam->last_capture_time_us,
        cam->capture_count,
        (double)infer->last_score,
        infer->last_decision,
        infer->trigger_latched ? "true" : "false",
        (unsigned)infer->baseline_frame_len,
        infer->sample_count,
        infer->trigger_count,
        infer->last_inference_time_us,
        infer->dropped_frame_count,
        state->event_count,
        evt->last_event_name,
        evt->last_event_ts_ms,
        (double)state->last_confidence,
        state->alert_active ? "true" : "false",
        evt->accepted_events,
        evt->ignored_events,
        EDGEGUARD_PERSON_DETECT_THRESHOLD,
        EDGEGUARD_PERSON_RESET_THRESHOLD,
        free_heap
    );

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    esp_err_t err = httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
    free(response);
    return err;
}

static esp_err_t events_get_handler(httpd_req_t *req)
{
    edgeguard_recent_event_t items[EDGEGUARD_EVENT_HISTORY_LEN];
    const size_t count = event_manager_get_recent_events(items, EDGEGUARD_EVENT_HISTORY_LEN);

    char *response = (char *)malloc(4096);
    if (response == NULL) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "alloc failed");
        return ESP_ERR_NO_MEM;
    }

    size_t used = 0;
    int n = snprintf(response, 4096, "{\"items\":[");
    if (n < 0) {
        free(response);
        return ESP_FAIL;
    }
    used = (size_t)n;

    for (size_t i = 0; i < count; ++i) {
        n = snprintf(
            response + used,
            4096 - used,
            "%s{"
            "\"timestamp_ms\":%" PRIu64 ","
            "\"name\":\"%s\","
            "\"confidence\":%.2f,"
            "\"accepted\":%s,"
            "\"is_test\":%s"
            "}",
            (i == 0) ? "" : ",",
            items[i].timestamp_ms,
            items[i].name,
            (double)items[i].confidence,
            items[i].accepted ? "true" : "false",
            items[i].is_test ? "true" : "false"
        );

        if (n < 0 || (size_t)n >= (4096 - used)) {
            free(response);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "events json overflow");
            return ESP_FAIL;
        }
        used += (size_t)n;
    }

    n = snprintf(response + used, 4096 - used, "]}");
    if (n < 0 || (size_t)n >= (4096 - used)) {
        free(response);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "events json overflow");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    esp_err_t err = httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
    free(response);
    return err;
}

static esp_err_t test_event_post_handler(httpd_req_t *req)
{
    esp_err_t err = event_manager_trigger_test_event();

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");

    if (err == ESP_OK) {
        return httpd_resp_sendstr(req, "{\"ok\":true,\"message\":\"test event queued\"}");
    }

    return httpd_resp_sendstr(req, "{\"ok\":false,\"message\":\"failed to queue test event\"}");
}

static esp_err_t capture_jpg_get_handler(httpd_req_t *req)
{
    const camera_service_status_t *cam = camera_service_get_status();

    if (cam->pixel_format != PIXFORMAT_JPEG) {
        httpd_resp_set_status(req, "503 Service Unavailable");
        httpd_resp_set_type(req, "text/plain");
        httpd_resp_send(
            req,
            "capture.jpg disabled in grayscale inference mode",
            HTTPD_RESP_USE_STRLEN
        );
        return ESP_ERR_NOT_SUPPORTED;
    }

    camera_fb_t *fb = camera_service_get_frame();
    if (fb == NULL) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "camera capture failed");
        return ESP_FAIL;
    }

    if (fb->format != PIXFORMAT_JPEG) {
        camera_service_return_frame(fb);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "frame is not jpeg");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "image/jpeg");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    esp_err_t err = httpd_resp_send(req, (const char *)fb->buf, fb->len);
    camera_service_return_frame(fb);
    return err;
}

static esp_err_t start_http_server(void)
{
    if (s_server != NULL) {
        return ESP_OK;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = EDGEGUARD_HTTP_SERVER_PORT;
    config.max_uri_handlers = 14;
    config.stack_size = 10240;

    ESP_RETURN_ON_ERROR(httpd_start(&s_server, &config), TAG, "httpd_start failed");

    httpd_uri_t root = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = root_get_handler,
        .user_ctx = NULL
    };

    httpd_uri_t favicon = {
        .uri = "/favicon.ico",
        .method = HTTP_GET,
        .handler = favicon_get_handler,
        .user_ctx = NULL
    };

    httpd_uri_t status = {
        .uri = "/api/status",
        .method = HTTP_GET,
        .handler = status_get_handler,
        .user_ctx = NULL
    };

    httpd_uri_t events = {
        .uri = "/api/events",
        .method = HTTP_GET,
        .handler = events_get_handler,
        .user_ctx = NULL
    };

    httpd_uri_t test_event = {
        .uri = "/api/test_event",
        .method = HTTP_POST,
        .handler = test_event_post_handler,
        .user_ctx = NULL
    };

    httpd_uri_t capture_jpg = {
        .uri = "/capture.jpg",
        .method = HTTP_GET,
        .handler = capture_jpg_get_handler,
        .user_ctx = NULL
    };

    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_server, &root), TAG, "register / failed");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_server, &favicon), TAG, "register /favicon.ico failed");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_server, &status), TAG, "register /api/status failed");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_server, &events), TAG, "register /api/events failed");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_server, &test_event), TAG, "register /api/test_event failed");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_server, &capture_jpg), TAG, "register /capture.jpg failed");

    ESP_LOGI(TAG, "dashboard started on http://%s:%d", network_manager_get_ip_addr(), EDGEGUARD_HTTP_SERVER_PORT);
    return ESP_OK;
}

static void stop_http_server(void)
{
    if (s_server != NULL) {
        ESP_LOGI(TAG, "stopping dashboard");
        httpd_stop(s_server);
        s_server = NULL;
    }
}

static void web_server_task(void *arg)
{
    (void)arg;

    while (1) {
        const bool connected = network_manager_is_connected();

        if (connected && s_server == NULL) {
            if (start_http_server() != ESP_OK) {
                ESP_LOGE(TAG, "failed to start dashboard");
            }
        } else if (!connected && s_server != NULL) {
            stop_http_server();
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

esp_err_t web_server_init(void)
{
    if (s_web_task_handle != NULL) {
        ESP_LOGW(TAG, "already initialized");
        return ESP_OK;
    }

    BaseType_t ok = xTaskCreate(
        web_server_task,
        "edgeguard_web",
        6144,
        NULL,
        4,
        &s_web_task_handle
    );

    ESP_RETURN_ON_FALSE(ok == pdPASS, ESP_ERR_NO_MEM, TAG, "failed to create web server task");

    ESP_LOGI(TAG, "dashboard task initialized");
    return ESP_OK;
}