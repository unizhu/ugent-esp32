# UGENT ESP32 Display App — Implementation Plan

**Date:** 2026-05-18
**Author:** UGENT
**Hardware:** ESP32-2432S028R (2.8" 320x240 ILI9341 + XPT2046 Touch)
**Framework:** Arduino + LVGL 8.x
**Status:** Plan — Ready for Review

---

## 1. Hardware Summary

Based on the board specification, schematics, and example code in this repo:

| Feature | Details |
|---------|---------|
| MCU | ESP32-WROOM-32 (Dual-core 240MHz, 520KB SRAM, 4MB Flash) |
| Display | 2.8" 320x240 ILI9341 TFT (SPI) |
| Touch | XPT2046 resistive touch (SPI) |
| Backlight | GPIO 21, PWM controllable |
| SD Card | SPI SD card slot |
| Other | DHT11 temp/humidity, Camera (OV2640 optional), Buzzer, LED, Relay |
| USB | Micro USB (power + programming) |

### Pin Mapping (from User_Setup.h and LVGL example)

```
TFT:
  MOSI  = GPIO 13
  SCLK  = GPIO 14
  CS    = GPIO 15
  DC    = GPIO 2
  RST   = GPIO 12
  BL    = GPIO 21

Touch (XPT2046):
  DOUT  = GPIO 39  (MISO)
  DIN   = GPIO 32  (MOSI)
  CS    = GPIO 33
  CLK   = GPIO 25

SD Card: shared SPI bus with TFT

Buttons: GPIO 0 (boot), GPIO 35 (on-board button)
```

### Touch Calibration (from LVGL example)

```
touch.setCal(526, 3443, 750, 3377, 320, 240, 1);
```

---

## 2. System Architecture

```
+---------------------------+         +---------------------------+
|      ESP32-2432S028R      |         |      ugent server         |
|                           |         |   (channel-web plugin)    |
|  +---------------------+  |  WiFi   |                           |
|  |  LVGL UI            |  | <-----> |  GET /v1/ugent/status     |
|  |  - Status Screen    |  |  HTTP   |  GET /v1/ugent/events SSE |
|  |  - WiFi Setup       |  |  SSE    |  POST /v1/ugent/commands  |
|  |  - Interaction      |  |         |  GET /v1/ugent/snapshot   |
|  |  - Task Monitor     |  |         |                           |
|  +---------------------+  |         +---------------------------+
|  |  App Logic          |  |
|  |  - WiFi Manager     |  |
|  |  - Status Poller    |  |
|  |  - SSE Client       |  |
|  |  - Input Handler    |  |
|  +---------------------+  |
|  |  HAL                |  |
|  |  - TFT_eSPI + Touch |  |
|  |  - NVS (EEPROM)     |  |
|  +---------------------+  |
+---------------------------+
```

---

## 3. UI Screen Design

The app uses LVGL with 5 screens, navigated via bottom tab bar or swipe.

### Screen 1: Dashboard (Home)

```
+------------------------------------------+
| UGENT Monitor          [WiFi icon] [BLE] |
+------------------------------------------+
|                                          |
|  Status:  [== RUNNING ==]   (green bar)  |
|                                          |
|  +-- Agent Status ---------------------+ |
|  | Session: 3 active                   | |
|  | Tasks:   2 running / 5 completed    | |
|  | Memory:  147 entries                | |
|  | Cron:    6 jobs, next in 2h 15m     | |
|  | Skills:  12 loaded                  | |
|  +-------------------------------------+ |
|                                          |
|  +-- Active Tasks ---------------------+ |
|  | > Fix DingTalk plugin    [RUNNING]  | |
|  | > Generate news report   [PENDING]  | |
|  +-------------------------------------+ |
|                                          |
|  +-- Needs Attention ------------------+ |
|  | ! Confirm: Run build?    [TAP]     | |
|  | ! Review: PR #42         [TAP]     | |
|  +-------------------------------------+ |
+------------------------------------------+
| [Home]  [Tasks]  [Interact]  [Settings]  |
+------------------------------------------+
```

### Screen 2: Task Monitor

```
+------------------------------------------+
| Task Monitor               [< Back]      |
+------------------------------------------+
|                                          |
|  Agent: active                           |
|  Sub-agents: 2                           |
|                                          |
|  +-- Task List -------------------------+|
|  | [scrollable list]                    ||
|  | > task-1  Fix plugin   [COMPLETE]   ||
|  | > task-2  Gen report   [RUNNING]    ||
|  | > task-3  Memory clean  [PENDING]   ||
|  | > task-4  Code review   [FAILED]    ||
|  +-------------------------------------+ |
|                                          |
|  Tap a task for details                  |
+------------------------------------------+
| [Home]  [Tasks]  [Interact]  [Settings]  |
+------------------------------------------+
```

### Screen 3: Interaction / Human-in-the-Loop

This is where the ESP32 becomes an interactive terminal.

```
+------------------------------------------+
| Interaction                 [< Back]      |
+------------------------------------------+
|                                          |
|  +-- Pending Interactions -------------+ |
|  |                                     | |
|  |  UGENT asks:                        | |
|  |  "Run cargo build with --release?"  | |
|  |                                     | |
|  |  [ Approve ]    [ Reject ]          | |
|  |                                     | |
|  +-------------------------------------+ |
|                                          |
|  +-- Send Command ---------------------+ |
|  | [text input area]                   | |
|  | [ Send ]                            | |
|  +-------------------------------------+ |
|                                          |
|  +-- Recent Messages ------------------+ |
|  | 15:32 Cron job completed             | |
|  | 15:30 Task 3 started                 | |
|  | 15:28 Need approval for build        | |
|  +-------------------------------------+ |
+------------------------------------------+
| [Home]  [Tasks]  [Interact]  [Settings]  |
+------------------------------------------+
```

### Screen 4: WiFi & BLE Settings

```
+------------------------------------------+
| WiFi Settings               [< Back]     |
+------------------------------------------+
|                                          |
|  Current: UNI-MikroTik                   |
|  IP: 192.168.1.105                       |
|  RSSI: -42 dBm  [====      ]            |
|                                          |
|  +-- Available Networks ---------------+ |
|  | > UNI-MikroTik     -42  [WPA2]     | |
|  | > TP-Link_5G       -58  [WPA2]     | |
|  | > Guest_WiFi       -71  [OPEN]     | |
|  +-------------------------------------+ |
|                                          |
|  Password: [********************]        |
|  [ Connect ]  [ SmartConfig ]            |
|                                          |
|  +-- BLE Pairing ----------------------+ |
|  | BLE Name: UGENT-ESP32               | |
|  | Status: Not connected               | |
|  | [ Start BLE ]  [ Pair ]             | |
|  +-------------------------------------+ |
|                                          |
|  +-- UGENT Server ---------------------+ |
|  | Host: 192.168.1.100                 | |
|  | Port: 8765                          | |
|  | API Key: [********************]     | |
|  | [ Test Connection ]                 | |
|  +-------------------------------------+ |
+------------------------------------------+
| [Home]  [Tasks]  [Interact]  [Settings]  |
+------------------------------------------+
```

### Screen 5: System Info

```
+------------------------------------------+
| System Info                 [< Back]     |
+------------------------------------------+
|                                          |
|  Firmware: UGENT-ESP32 v1.0.0           |
|  LVGL: 8.3.0                             |
|  Heap: 142KB free / 520KB total          |
|  PSRAM: not available                    |
|  Uptime: 2h 34m                          |
|  Flash: 4MB (Partition: Huge APP 3MB)    |
|                                          |
|  [ Restart ]  [ Factory Reset ]          |
+------------------------------------------+
| [Home]  [Tasks]  [Interact]  [Settings]  |
+------------------------------------------+
```

---

## 4. Network Communication Design

### 4.1 WiFi Connection Flow

```
Boot
  |
  +-- Check NVS for saved SSID/password
  |     |
  |     +-- Found --> Auto-connect (up to 10s timeout)
  |     |              |
  |     |              +-- Success --> Start main app
  |     |              +-- Fail --> Show WiFi setup screen
  |     |
  |     +-- Not found --> Show WiFi setup screen
  |
  +-- WiFi setup screen offers 3 methods:
        |
        +-- Scan & Select (touch UI)
        +-- SmartConfig (ESP-Touch app)
        +-- BLE provisioning (optional)
```

### 4.2 UGENT Server Connection

The ESP32 connects to ugent's `channel-web` plugin. Settings stored in NVS:

| Key | Default | Description |
|-----|---------|-------------|
| `ugent_host` | `192.168.1.100` | ugent channel-web IP |
| `ugent_port` | `8765` | channel-web port |
| `ugent_api_key` | (empty) | API key for auth |
| `poll_interval` | `5000` | Status poll interval (ms) |

### 4.3 Data Flow

```
ESP32                              ugent channel-web
  |
  +-- GET /v1/ugent/status ------->  StatusProvider JSON
  |   (every 5s, or on event)       {version, session, tasks, cron, ...}
  |
  +-- GET /v1/ugent/events SSE --->  AgentEvent stream
  |   (persistent connection)        {type, payload, severity, ...}
  |
  +-- POST /v1/ugent/commands ---->  SlashCommandRequest
  |   (user interaction)             {command: "/approve", args: "..."}
  |
  +-- GET /v1/ugent/snapshot ------>  SessionSnapshotResponse
      (on demand)                    {session_id, messages: [...]}
```

### 4.4 SSE Event Handling

The ESP32 will maintain a persistent SSE connection to `/v1/ugent/events`. Key event types to handle:

| AgentEvent Type | UI Action |
|-----------------|-----------|
| `TaskStatusChanged` | Update task list, flash indicator |
| `InteractionRequest` | Show interaction dialog, buzz/vibrate |
| `TurnStarted` | Update agent status to "Running" |
| `TurnCompleted` | Update agent status to "Idle" |
| `ToolCallStarted` | Show activity spinner |
| `ChatNotification` | Show notification toast |
| `CronJobTriggered` | Update cron status |
| `MemoryUpdated` | Update memory count |

---

## 5. Implementation Phases

### Phase 1: Bootstrapping and Hardware Bring-up (Week 1)

**Goal:** Get the ESP32 flashing with LVGL, touch working, and basic UI skeleton.

#### 1.1 Development Environment Setup
- Install Arduino IDE 2.x or PlatformIO
- Install ESP32 board support (v2.0+)
- Install libraries:
  - `TFT_eSPI` (with the board's User_Setup.h)
  - `lvgl` v8.3.x
  - `TFT_Touch`
  - `ArduinoJson` v7
  - `WiFi` (built-in)
  - `BLEDevice` (built-in)
  - `HTTPClient` (built-in)
- Configure `User_Setup.h` with correct pin definitions from this repo
- Set Partition Scheme to "Huge APP (3MB No OTA / 1MB SPIFFS)"

#### 1.2 LVGL Skeleton
- Initialize TFT_eSPI with ILI9341_2_DRIVER, 320x240 landscape
- Initialize XPT2046 touch with calibration `setCal(526, 3443, 750, 3377, 320, 240, 1)`
- Create LVGL display driver with `my_disp_flush` and `my_touchpad_read` (copy from LVGL example)
- Set `lv_disp_draw_buf_init` with buffer `screenWidth * 10` (as per example)
- Create tab-based navigation skeleton with 5 empty screens

#### 1.3 NVS Storage
- Use `Preferences.h` (ESP32 native NVS, not EEPROM)
- Store: WiFi SSID, WiFi password, ugent host, port, API key, brightness
- Read on boot, write on user change

**Deliverable:** Firmware boots, shows LVGL tabs, touch works, brightness stored in NVS.

---

### Phase 2: WiFi Manager (Week 1-2)

**Goal:** Reliable WiFi connection with multiple provisioning methods.

#### 2.1 Auto-Connect
- Read SSID/password from NVS
- Connect with `WiFi.begin(ssid, pass)` + `WiFi.setAutoReconnect(true)`
- 10-second timeout, show progress bar (adapt from SmallDesktopDisplay example)
- On success: save to NVS, proceed to main app

#### 2.2 Scan & Select
- `WiFi.scanNetworks()` to populate LVGL dropdown
- User taps network, enters password via LVGL keyboard
- Test connection, save to NVS

#### 2.3 SmartConfig Fallback
- If no saved credentials or auto-connect fails
- Show QR code screen (adapt from SmallDesktopDisplay QR example)
- `WiFi.beginSmartConfig()` — user configures via ESP-Touch app
- On success: save SSID/password to NVS

#### 2.4 Connection Monitoring
- `WiFi.onStationModeDisconnected()` callback → auto-reconnect
- Show WiFi status icon in header (connected/disconnected/connecting)
- Display RSSI signal strength bars

**Deliverable:** WiFi connects reliably via any method, credentials persist, auto-reconnect works.

---

### Phase 3: UGENT Status Client (Week 2-3)

**Goal:** Fetch and display ugent status from `/v1/ugent/status`.

#### 3.1 HTTP Client
- `HTTPClient` with GET to `http://{host}:{port}/v1/ugent/status`
- Set `x-api-key` header from NVS
- Parse JSON response with `ArduinoJson`
- Map to internal status struct:

```cpp
struct UgentStatus {
  char version[16];
  bool agent_running;
  int sessions_active;
  int tasks_running;
  int tasks_completed;
  int tasks_failed;
  int memory_entries;
  int cron_jobs;
  char next_cron[32];
  int skills_loaded;
  bool swarm_active;
  char swarm_role[16];
};
```

#### 3.2 Status Polling
- Poll every 5 seconds (configurable)
- Update LVGL labels/meters on change
- Use `lv_msg` (LVGL messaging) to decouple data from UI

#### 3.3 Connection Setup Screen
- Settings screen with LVGL keyboard input for:
  - UGENT host IP
  - UGENT port
  - API key
- "Test Connection" button → HTTP GET `/v1/health`
- Save to NVS on success

**Deliverable:** Dashboard screen shows live ugent status data.

---

### Phase 4: SSE Real-Time Events (Week 3-4)

**Goal:** Persistent SSE connection for real-time updates without polling.

#### 4.1 SSE Client
- Use `WiFiClient` + manual SSE parsing (lightweight, no extra library)
- Connect to `GET /v1/ugent/events?route_key=esp32&session_id={id}`
- Set headers: `x-api-key`, `Accept: text/event-stream`
- Parse SSE frames: `event:`, `data:`, `id:`, `retry:`
- Handle keep-alive `:` comments
- Auto-reconnect on disconnect with exponential backoff

#### 4.2 Event Processing
- Parse `data:` JSON with `ArduinoJson`
- Extract `event_type` and `severity`
- Route to UI:
  - `TaskStatusChanged` → update task list immediately (no poll needed)
  - `InteractionRequest` → show dialog, buzzer alert
  - `TurnStarted/Completed` → update status bar
  - `ChatNotification` → show toast notification

#### 4.3 Dual-Mode Operation
- SSE connected: event-driven updates (low latency)
- SSE disconnected: fall back to HTTP polling (resilient)
- Show connection status indicator in header

**Deliverable:** Real-time status updates via SSE, with polling fallback.

---

### Phase 5: Interaction & Human-in-the-Loop (Week 4-5)

**Goal:** Let user approve/reject ugent interaction requests from the ESP32 screen.

#### 5.1 Interaction Dialog
- When `InteractionRequest` event arrives:
  - Show modal dialog with the question text
  - Two buttons: "Approve" / "Reject"
  - Optional text input for custom response
  - Buzzer beep to get attention

#### 5.2 Command Sending
- POST to `/v1/ugent/commands` with body:
```json
{
  "command": "/approve",
  "args": "task-123",
  "session_id": "current-session"
}
```
- Or for text input:
```json
{
  "command": "/interact",
  "args": "user typed response"
}
```

#### 5.3 Quick Actions
- Pre-defined action buttons on Interact screen:
  - "Approve" — send `/approve`
  - "Reject" — send `/reject`
  - "Yes" — send `/yes`
  - "No" — send `/no`
  - Custom text input with LVGL keyboard

#### 5.4 Message History
- Keep last 20 interaction messages in ring buffer
- Show in scrollable list on Interact screen
- Timestamp each message

**Deliverable:** User can interact with ugent from the ESP32 touch screen.

---

### Phase 6: Task Monitor (Week 5)

**Goal:** Detailed task view with live status updates.

#### 6.1 Task List
- Parse `tasks` array from status JSON
- Show each task as LVGL list item with:
  - Task name (truncated)
  - Status badge (running=green, completed=blue, failed=red, pending=gray)
  - Progress bar if available

#### 6.2 Task Details
- Tap a task → show detail panel:
  - Full name and description
  - Status and progress
  - Creation time
  - Dependencies

#### 6.3 Sub-Agent View
- If `swarm.active`, show swarm status:
  - Node name and role
  - Active sub-agents count
  - Remote task queue length

**Deliverable:** Full task monitoring with real-time updates.

---

### Phase 7: BLE Provisioning & Companion (Week 5-6)

**Goal:** BLE for initial setup and as a companion channel.

#### 7.1 BLE Service
- Create BLE server with UGENT-specific service UUID
- Characteristics:
  - WiFi SSID (read/write)
  - WiFi Password (write only)
  - UGENT Host (read/write)
  - UGENT API Key (write only)
  - Status (notify/read) — push status updates to phone
  - Command (write) — send commands from phone via BLE

#### 7.2 BLE Setup Flow
- User can configure WiFi and ugent settings via BLE from a phone app
- ESP32 advertises as "UGENT-ESP32"
- On BLE write of credentials → connect WiFi → notify success/failure

#### 7.3 BLE Companion
- After WiFi is connected, BLE remains active
- Phone app can monitor status and send commands via BLE
- ESP32 pushes notifications (interaction requests) to phone via BLE notify

**Deliverable:** BLE provisioning and companion mode working.

---

### Phase 8: Polish & Optimization (Week 6)

**Goal:** Production-quality firmware.

#### 8.1 Memory Optimization
- Monitor heap with `ESP.getFreeHeap()`
- Use `ps_malloc()` where possible (not available on ESP32 non-S3, so be careful)
- LVGL buffer: use `screenWidth * 10` as per example (not full frame buffer)
- ArduinoJson: use fixed-size documents, avoid `JsonDocument` on heap
- String handling: use `char[]` buffers, avoid `String` class

#### 8.2 Power Management
- Screen brightness control via PWM on GPIO 21
- Auto-dim after 60 seconds of no touch
- Wake on touch event
- Deep sleep option with wake on touch or timed interval

#### 8.3 OTA Update (if SPIFFS space allows)
- Store firmware URL in NVS
- `HTTPUpdate` library for over-the-air updates
- Manual trigger from System Info screen

#### 8.4 Error Handling
- Watchdog timer for hang detection
- Graceful WiFi reconnect without UI freeze
- JSON parse error handling
- HTTP timeout handling (5s)
- NVS corruption detection

#### 8.5 UI Polish
- Smooth transitions between screens
- Color scheme: dark background (low power OLED-friendly)
- Status indicators: colored dots for running/failed/idle
- Font: use built-in LVGL fonts, avoid custom fonts to save flash

---

## 6. Project Structure

```
ugent-esp32/
├── docs/
│   └── ugent-esp32-app-plan.md        (this file)
├── firmware/
│   └── ugent-monitor/
│       ├── ugent-monitor.ino          (main entry point)
│       ├── config.h                   (pin definitions, constants)
│       ├── wifi_manager.h/.cpp        (WiFi connect, scan, SmartConfig)
│       ├── ugent_client.h/.cpp        (HTTP status, SSE events)
│       ├── ble_service.h/.cpp         (BLE provisioning + companion)
│       ├── nvs_storage.h/.cpp         (Preferences wrapper)
│       ├── ui/
│       │   ├── ui_manager.h/.cpp      (screen navigation, LVGL init)
│       │   ├── screen_dashboard.h/.cpp
│       │   ├── screen_tasks.h/.cpp
│       │   ├── screen_interact.h/.cpp
│       │   ├── screen_settings.h/.cpp
│       │   ├── screen_system.h/.cpp
│       │   └── ui_theme.h             (colors, fonts, styles)
│       └── lib/
│           └── (Symlink or copy from vendor libraries/)
├── 2.8inch_ESP32-2432S028R/           (vendor reference, do not modify)
│   ├── 1-Demo/
│   ├── 2-Specification/
│   ├── ...
│   └── 6-User_Manual/
└── README.md
```

---

## 7. Library Dependencies

| Library | Version | Purpose | Size Impact |
|---------|---------|---------|-------------|
| TFT_eSPI | latest | ILI9341 display driver | ~30KB |
| lvgl | 8.3.x | UI framework | ~120KB |
| TFT_Touch | latest | XPT2046 touch driver | ~5KB |
| ArduinoJson | 7.x | JSON parse/serialize | ~20KB |
| WiFi | built-in | STA/AP connection | built-in |
| BLEDevice | built-in | BLE server | ~40KB |
| HTTPClient | built-in | REST API calls | built-in |
| Preferences | built-in | NVS key-value store | built-in |
| ESPmDNS | built-in | mDNS discovery (optional) | ~5KB |

**Estimated total flash usage:** ~250KB of 3MB available (comfortable margin).

---

## 8. Key Design Decisions

### 8.1 Arduino Framework (not ESP-IDF)

**Why:** All vendor examples use Arduino. LVGL Arduino integration is mature. The SmallDesktopDisplay example proves this board works well with Arduino + TFT_eSPI. The LVGL example uses `lv_demo_widgets()` which confirms full LVGL compatibility.

**Trade-off:** Less control over FreeRTOS tasks than ESP-IDF, but simpler development and all examples are Arduino-based.

### 8.2 LVGL 8.x (not 9.x)

**Why:** Vendor examples ship LVGL 8.x. The LVGL Arduino demo uses `lv_disp_drv_t` API which is LVGL 8.x style. LVGL 9.x has breaking API changes and fewer Arduino examples.

### 8.3 TFT_eSPI (not LovyanGFX)

**Why:** The vendor LVGL example uses TFT_eSPI with `ILI9341_2_DRIVER`. The User_Setup.h pin configuration is provided for TFT_eSPI. LovyanGFX is available as an alternative but TFT_eSPI has better LVGL integration via the provided flush callback pattern.

### 8.4 SSE over WebSocket

**Why:** The channel-web plugin already has an SSE endpoint (`/v1/ugent/events`) with proper keep-alive, event IDs, and 250ms polling from the server side. SSE is simpler to parse on ESP32 than WebSocket — just a plain HTTP GET with line-by-line parsing. No need for a WebSocket library.

### 8.5 Preferences (NVS) over EEPROM

**Why:** `Preferences.h` uses ESP32's native NVS (Non-Volatile Storage) which is more reliable than the EEPROM emulation used in the vendor examples. It supports typed key-value pairs and namespaces.

### 8.6 Dual-Mode: SSE + Polling

**Why:** SSE provides real-time updates but can disconnect. The polling fallback (every 5s) ensures the display stays current even if SSE fails. This provides both responsiveness and reliability.

---

## 9. UGENT Server Endpoints Used

| Endpoint | Method | Purpose | Auth |
|----------|--------|---------|------|
| `/v1/ugent/status` | GET | Full status JSON (Phase 1.3 - COMPLETED in ugent codebase) | x-api-key |
| `/v1/ugent/events` | GET (SSE) | Real-time event stream | x-api-key |
| `/v1/ugent/commands` | POST | Execute slash commands, approve/reject | x-api-key |
| `/v1/ugent/snapshot` | GET | Session message history | x-api-key |
| `/v1/health` | GET | Connection test | x-api-key |

### Status JSON Structure (from UgentStatus)

```json
{
  "version": "0.1.8",
  "workspace": "/root/ugent-playground/ugent",
  "session": {
    "active_sessions": 3,
    "current_session_id": "abc123"
  },
  "tasks": {
    "running": 2,
    "completed": 15,
    "failed": 1,
    "pending": 3,
    "task_list": [
      {"id": "t1", "name": "Fix plugin", "status": "running", "progress": 60},
      {"id": "t2", "name": "Gen report", "status": "pending", "progress": 0}
    ]
  },
  "memory": {
    "total_entries": 147,
    "scopes": ["default", "workspace"]
  },
  "cron": {
    "total_jobs": 6,
    "next_run": "2026-05-19T05:00:00+08:00",
    "job_list": [...]
  },
  "skills": {
    "loaded": 12
  },
  "plugins": {
    "running": 3,
    "plugin_list": [...]
  },
  "swarm": {
    "active": false
  }
}
```

---

## 10. Timeline

| Phase | Duration | Description |
|-------|----------|-------------|
| Phase 1 | 3-4 days | Bootstrap, LVGL skeleton, touch, NVS |
| Phase 2 | 3-4 days | WiFi manager (auto, scan, SmartConfig) |
| Phase 3 | 4-5 days | UGENT status client, dashboard screen |
| Phase 4 | 4-5 days | SSE real-time events |
| Phase 5 | 4-5 days | Interaction / human-in-the-loop |
| Phase 6 | 2-3 days | Task monitor detail view |
| Phase 7 | 3-4 days | BLE provisioning |
| Phase 8 | 3-4 days | Polish, optimization, OTA |
| **Total** | **4-6 weeks** | |

---

## 11. Risk Assessment

| Risk | Impact | Mitigation |
|------|--------|------------|
| Memory pressure (520KB SRAM) | High | Monitor heap, use fixed buffers, avoid String class, LVGL buffer size tuning |
| WiFi instability | Medium | Auto-reconnect with backoff, show status, polling fallback |
| SSE connection drops | Medium | Auto-reconnect, fall back to polling, show connection status |
| Touch calibration drift | Low | Store calibration in NVS, allow recalibration in settings |
| JSON parse failures | Low | Use ArduinoJson with error checking, skip malformed frames |
| Flash space (3MB) | Low | Estimated 250KB usage, plenty of margin |
| No PSRAM on ESP32-WROOM-32 | Medium | Cannot cache large images, use streamed rendering |

---

## 12. Getting Started Checklist

- [ ] Install Arduino IDE 2.x or PlatformIO
- [ ] Install ESP32 board support (v2.0+)
- [ ] Copy `User_Setup.h` from vendor LVGL example to TFT_eSPI library
- [ ] Install LVGL 8.3.x library
- [ ] Install TFT_Touch library
- [ ] Install ArduinoJson 7.x
- [ ] Set Partition Scheme to "Huge APP (3MB No OTA / 1MB SPIFFS)"
- [ ] Flash LVGL_Arduino.ino example to verify hardware works
- [ ] Flash Touch_Test example to verify touch works
- [ ] Create firmware project structure
- [ ] Begin Phase 1

---

## 13. Reference Files in This Repo

| File | Purpose |
|------|---------|
| `2-Specification/ESP32-2432S028 Specifications-EN.pdf` | Full hardware specification |
| `4-Driver_IC_Data_Sheet/ESP32-WROOM-32.PDF` | ESP32 module datasheet |
| `4-Driver_IC_Data_Sheet/JC2432A028N.pdf` | LCD module datasheet |
| `5-Schematic/ESP32-2432S028-MCU.jpg` | MCU schematic |
| `5-Schematic/ESP32-2432S028-LCM.jpg` | LCD schematic |
| `5-Schematic/ESP32-WROOM-1 Pin definition.png` | Pin mapping reference |
| `6-User_Manual/Getting started 2.8 Inch.pdf` | Getting started guide |
| `1-Demo/Demo_Arduino/3_4-4_2.8-LVGL_Arduino/` | LVGL + TFT_eSPI + Touch reference |
| `1-Demo/Demo_Arduino/4_11_SmallDesktopDisplay/` | WiFi desktop display reference |
| `1-Demo/Demo_Arduino/4_2_wifi_STA/` | WiFi STA example |
| `1-Demo/Demo_Arduino/4_3_wifi_SmartConfig/` | SmartConfig WiFi provisioning |
| `1-Demo/Demo_Arduino/5_1_bleService/` | BLE service example |
| `1-Demo/Demo_Arduino/4_5_WIFI_STA_TCP_Client/` | TCP client reference |
| `3-Structure_Diagram/Dimensions.png` | Board dimensions |
