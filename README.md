# UGENT ESP32 Monitor

A real-time status display and human-in-the-loop interaction device for [UGENT](https://github.com/ugent), running on the **ESP32-2432S028R** (Cheap Yellow Display) — a 2.8" TFT touchscreen board.

## Overview

The UGENT ESP32 Monitor connects to your UGENT server via WiFi and displays:

- **Dashboard** — Agent status, active sessions, memory stats, cron jobs
- **Tasks** — Real-time task list with status (running, completed, failed, pending)
- **Interaction** — Human-in-the-loop approval/rejection of agent actions, text input
- **Settings** — WiFi configuration, UGENT server address, API key, display brightness

Communication uses HTTP polling (5s interval) with SSE (Server-Sent Events) for real-time event streaming.

### v1.4.0 Changes (current)

- **Fixed pin assignments based on witnessmenow/ESP32-Cheap-Yellow-Display** — GPIO 12 is TFT MISO, NOT reset. The display reset is tied to board reset (`TFT_RST -1`). Previous configs wrongly assigned GPIO 12 as `TFT_RST`, causing color corruption.
- **Display on HSPI, touch on separate VSPI** — `USE_HSPI_PORT` is now correctly defined. Display uses HSPI (SPI2), touch uses a separate `SPIClass(VSPI)` instance with XPT2046_Touchscreen library.
- **Replaced TFT_Touch with XPT2046_Touchscreen** — The TFT_Touch library uses bit-banged GPIO which conflicts with TFT_eSPI's SPI. XPT2046_Touchscreen uses proper hardware SPI on the separate VSPI bus.
- **SPI frequency 55MHz** — Matches witnessmenow reference, reliable on all board revisions.

### v1.3.0 Changes (superseded by v1.4.0)

- Rewrote to match Waveshare vendor LVGL demo — but vendor's pin mapping was WRONG
- GPIO 12 was incorrectly labeled as TFT_RST in vendor docs; it's actually TFT_MISO

### v1.2.0 Changes (superseded)

- Attempted HSPI/VSPI SPI bus separation — direction was correct but pin mapping was wrong

## Hardware

### Required Board

| Item | Details |
|------|---------|
| **Board** | ESP32-2432S028R (2.8" TFT LCD + Touch) |
| **MCU** | ESP32-WROOM-32 (Dual-core 240MHz, 520KB SRAM, 4MB Flash) |
| **Display** | 2.8" 320×240 ILI9341 TFT (SPI) |
| **Touch** | XPT2046 resistive touch (SPI) |
| **Backlight** | GPIO 21, PWM controllable |
| **USB** | Micro USB (power + programming) |
| **Aliases** | "Cheap Yellow Display" / "CYD" / "ESP32-2432S028R" |

### Where to Purchase

- AliExpress: search "ESP32-2432S028R" or "ESP32 2.8 inch TFT"
- Amazon: search "ESP32-2432S028R display"
- eBay: search "ESP32 2432S028R 2.8 TFT"
- Price range: approximately $8–15 USD

### Pin Mapping

```
TFT Display (ILI9341):
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
  IRQ   = GPIO 36

Other:
  Boot Button   = GPIO 0
  On-board LED  = GPIO TBD (board variant dependent)
```

## Prerequisites

### USB Driver

The ESP32-2432S028R uses either a **CP210x** or **CH9102F** USB-to-serial chip. Install the driver for your OS:

| OS | Driver | Download |
|----|--------|----------|
| Windows | CP210x VCP | [Silicon Labs CP210x Drivers](https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers) |
| Windows | CH9102F | [WCH CH9102F Driver](http://www.wch-ic.com/downloads/CH341SER_EXE.html) |
| macOS | Built-in (CP210x) | No driver needed on macOS 10.15+ |
| macOS | CH9102F | [WCH CH9102F Driver for macOS](http://www.wch-ic.com/downloads/CH34xSER_MAC_ZIP.html) |

> **Tip:** If the board is not recognized, try the other driver. You can identify the chip by looking at the small IC near the USB port.

---

## Method 1: Arduino IDE (Recommended for Beginners)

### Step 1 — Install Arduino IDE

Download and install **Arduino IDE 2.3.x** (latest stable as of 2026):

- **Official download:** [https://www.arduino.cc/en/software](https://www.arduino.cc/en/software)
- **Windows:** Download the `.exe` installer or use the Microsoft Store version
- **macOS:** Download the `.dmg` file, drag to Applications

### Step 2 — Add ESP32 Board Support

1. Open Arduino IDE
2. Go to **File > Preferences** (macOS: **Arduino IDE > Settings**)
3. In **"Additional boards manager URLs"**, paste:

```
https://espressif.github.io/arduino-esp32/package_esp32_index.json
```

If you already have other URLs, separate them with commas or click the icon next to the field to add a new line.

4. Click **OK**
5. Go to **Tools > Board > Boards Manager...**
6. Search for **"esp32"**
7. Install **"esp32 by Espressif Systems"** (version 3.x recommended)

### Step 3 — Install Required Libraries

Open **Sketch > Include Library > Manage Libraries...** and install:

| Library | Author | Version | Notes |
|---------|--------|---------|-------|
| **TFT_eSPI** | Bodmer | latest | TFT display driver for ILI9341 |
| **lvgl** | LVGL | **8.x** (NOT 9.x) | UI framework — must use v8 |
| **XPT2046_Touchscreen** | Paul Stoffregen | latest | Touch driver — separate VSPI bus |
| **ArduinoJson** | Benoit Blanchon | 7.x | JSON parsing |

> **Important:** LVGL 9.x has a completely different API. You MUST install LVGL 8.x. In Library Manager, search "lvgl", click the version dropdown, and select the latest 8.x release (e.g., 8.4.0).
>
> **XPT2046_Touchscreen library:** Install from Library Manager — search "XPT2046" by Paul Stoffregen. This uses hardware SPI on a separate VSPI bus, avoiding conflicts with the display HSPI bus.

### Step 4 — Configure TFT_eSPI (User_Setup.h)

TFT_eSPI requires a custom `User_Setup.h` to match the ESP32-2432S028R pin mapping.

**Locate the TFT_eSPI library folder:**

- **Windows:** `%USERPROFILE%\Documents\Arduino\libraries\TFT_eSPI\`
- **macOS:** `~/Documents/Arduino/libraries/TFT_eSPI/`

**Option A — Use the provided User_Setup.h (Recommended)**

A pre-configured `User_Setup.h` is included in this repo at `firmware/User_Setup.h`.
Copy it to the TFT_eSPI library folder:

```bash
# macOS
cp ugent-esp32/firmware/User_Setup.h ~/Documents/Arduino/libraries/TFT_eSPI/User_Setup.h

# Windows (PowerShell)
copy ugent-esp32\firmware\User_Setup.h "%USERPROFILE%\Documents\Arduino\libraries\TFT_eSPI\User_Setup.h"
```

**Key settings (based on witnessmenow/ESP32-Cheap-Yellow-Display):**
- `#define ILI9341_2_DRIVER` — Alternative ILI9341 driver (better compatibility)
- `#define TFT_MISO 12` — GPIO 12 is display MISO (NOT reset!)
- `#define TFT_RST -1` — Reset tied to board reset (not a GPIO)
- `#define USE_HSPI_PORT` — Display on HSPI (SPI2), touch on separate VSPI
- `#define SPI_FREQUENCY 55000000` — 55MHz display SPI
- No `TOUCH_CS` — Touch uses separate XPT2046_Touchscreen library on VSPI

**Option B — Manual configuration**

If you prefer to edit manually, the minimum required settings are:

```cpp
#define ILI9341_2_DRIVER
#define TFT_WIDTH  240
#define TFT_HEIGHT 320
#define TFT_MISO 12
#define TFT_MOSI 13
#define TFT_SCLK 14
#define TFT_CS   15
#define TFT_DC    2
#define TFT_RST  -1
#define TFT_BL   21
#define TFT_BACKLIGHT_ON HIGH
#define USE_HSPI_PORT
#define SPI_FREQUENCY         55000000
#define SPI_READ_FREQUENCY    20000000
#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF
#define SMOOTH_FONT
```

### Step 5 — Configure LVGL

> **CRITICAL:** The `lv_conf.h` file must be placed **next to** the `lvgl` folder in your `libraries` directory — NOT inside it.
>
> This is the #1 cause of `lv_conf.h: No such file or directory` errors.

**A pre-configured `lv_conf.h` is included in this repo** at `firmware/lv_conf.h`.

**Option A — Use the pre-configured file (Recommended)**

Copy the included `lv_conf.h` to your Arduino libraries folder:

```bash
# macOS
cp ugent-esp32/firmware/lv_conf.h ~/Documents/Arduino/libraries/lv_conf.h

# Windows (PowerShell)
copy ugent-esp32\firmware\lv_conf.h "%USERPROFILE%\Documents\Arduino\libraries\lv_conf.h"
```

**Option B — Create manually from template**

1. Find `lv_conf_template.h` inside the lvgl library folder
2. Copy it to `libraries/lv_conf.h` (one level UP from the lvgl folder)
3. Open it and change the very first `#if 0` to `#if 1`

**Correct directory layout:**

```
Documents/
  Arduino/
    libraries/
      lvgl/              ← LVGL library folder
      TFT_eSPI/          ← TFT driver
      ArduinoJson/        ← JSON library
      ...
      lv_conf.h           ← PLACE IT HERE (next to lvgl folder)
```

**Common mistake (WRONG):**

```
libraries/
  lvgl/
    lv_conf.h           ← WRONG! Do NOT put it inside lvgl folder
```

The reason: LVGL's `lv_conf_internal.h` resolves `../../lv_conf.h` relative to its own location (`lvgl/src/`), which points to the `libraries/` folder.

### Step 6 — Open and Configure the Firmware

1. Open Arduino IDE
2. Go to **File > Open** and navigate to:
   ```
   ugent-esp32/firmware/ugent-monitor/ugent-monitor.ino
   ```
3. Configure the board settings:

| Setting | Value |
|---------|-------|
| **Board** | ESP32 Dev Module |
| **Upload Speed** | 460800 or 921600 |
| **CPU Frequency** | 240MHz (WiFi/BT) |
| **Flash Frequency** | 80MHz |
| **Flash Mode** | QIO |
| **Flash Size** | 4MB |
| **Partition Scheme** | Huge APP (3MB No OTA/1MB SPIFFS) |
| **PSRAM** | Disabled |

4. Select the correct **Port**:
   - **Windows:** `COM3`, `COM4`, etc. (check Device Manager > Ports)
   - **macOS:** `/dev/cu.SLAB_USBtoUART` or `/dev/cu.usbserial-*`

### Step 7 — Build and Upload

1. Click the **Verify** button (checkmark icon) to compile
2. Click the **Upload** button (right-arrow icon) to flash
3. Wait for "Hard resetting via RTS pin..." message — upload is complete

### Step 8 — First Boot

1. Open **Tools > Serial Monitor** (set baud rate to `115200`)
2. You should see:

```
=== UGENT ESP32 Monitor ===
[UGENT] Boot
[UGENT] Hardware OK
[UGENT] LVGL OK
[UGENT] No WiFi credentials
[UGENT] Ready!
```

3. The touchscreen will show the **Settings** tab first since no WiFi credentials are stored
4. Tap **WiFi** to scan networks, select yours, and enter the password
5. Tap **Server** to set your UGENT server address (e.g., `192.168.1.100`) and port (default `8765`)
6. The device will connect and start displaying status

---

## Method 2: VS Code + Arduino CLI (For Advanced Users)

### Step 1 — Install Prerequisites

**Windows:**

1. Install [VS Code](https://code.visualstudio.com/)
2. Install the **"Arduino CLI (Minimal)"** extension from the VS Code Marketplace (extension ID: `oakiot.vscode-arduino-cli`)
   - Alternative: the **"Arduino"** extension by Microsoft (extension ID: `ms-vscode.vscode-arduino`)
3. The extension will prompt you to download **Arduino CLI** — accept, or install manually:
   ```powershell
   # Using winget
   winget install Arduino.ArduinoCLI

   # Or download from https://github.com/arduino/arduino-cli/releases
   ```

**macOS:**

1. Install VS Code: `brew install --cask visual-studio-code`
2. Install Arduino CLI:
   ```bash
   brew install arduino-cli
   ```
3. Install the VS Code **"Arduino CLI (Minimal)"** extension

### Step 2 — Initialize Arduino CLI

```bash
# Initialize config
arduino-cli config init

# Add ESP32 board URL
arduino-cli config add board_manager.additional_urls \
  https://espressif.github.io/arduino-esp32/package_esp32_index.json

# Update index
arduino-cli core update-index

# Install ESP32 core
arduino-cli core install esp32:esp32

# Install required libraries
arduino-cli lib install TFT_eSPI
arduino-cli lib install "lvgl"  # Install then pin to 8.x if needed
arduino-cli lib install ArduinoJson
```

> **Note:** If `arduino-cli lib install lvgl` installs LVGL 9.x, you may need to manually download LVGL 8.x from [https://github.com/lvgl/lvgl/releases](https://github.com/lvgl/lvgl/releases) and extract it into your libraries folder.

### Step 3 — Configure TFT_eSPI and LVGL

Same as **Method 1, Steps 4–5**. Edit the files in:

- **Windows:** `%USERPROFILE%\Documents\Arduino\libraries\`
- **macOS:** `~/Documents/Arduino/libraries/`

### Step 4 — Open the Project in VS Code

```bash
cd ugent-esp32/firmware/ugent-monitor
code .
```

### Step 5 — Build

**Using Arduino CLI directly:**

```bash
arduino-cli compile \
  --fqbn esp32:esp32:esp32 \
  --build-property build.partitions=huge_app.csv \
  --build-property upload.flash_size=4MB \
  ugent-monitor.ino
```

**Or using VS Code tasks:**

Create `.vscode/tasks.json`:

```json
{
  "version": "2.0.0",
  "tasks": [
    {
      "label": "Build",
      "type": "shell",
      "command": "arduino-cli compile --fqbn esp32:esp32:esp32 --build-property build.partitions=huge_app.csv .",
      "problemMatcher": ["$gcc"],
      "group": { "kind": "build", "isDefault": true }
    },
    {
      "label": "Upload",
      "type": "shell",
      "command": "arduino-cli upload -p ${input:serialPort} --fqbn esp32:esp32:esp32 .",
      "problemMatcher": []
    }
  ],
  "inputs": [
    {
      "id": "serialPort",
      "type": "promptString",
      "description": "Serial port (e.g. COM3 or /dev/cu.SLAB_USBtoUART)",
      "default": "/dev/cu.SLAB_USBtoUART"
    }
  ]
}
```

### Step 6 — Upload

```bash
# Find your serial port
arduino-cli board list

# Upload (replace PORT with your serial port)
# Windows:
arduino-cli upload -p COM3 --fqbn esp32:esp32:esp32 .

# macOS:
arduino-cli upload -p /dev/cu.SLAB_USBtoUART --fqbn esp32:esp32:esp32 .
```

### Step 7 — Serial Monitor

```bash
# Monitor serial output
# Windows:
arduino-cli monitor -p COM3 -c baudrate=115200

# macOS:
arduino-cli monitor -p /dev/cu.SLAB_USBtoUART -c baudrate=115200
```

Or use the VS Code Serial Monitor extension.

---

## Project Structure

```
ugent-esp32/
├── README.md                          ← This file
├── docs/
│   └── ugent-esp32-app-plan.md       ← Architecture and implementation plan
├── firmware/
│   ├── lv_conf.h                     ← Pre-configured LVGL 8.x config (copy to libraries/)
│   └── ugent-monitor/
│       ├── ugent-monitor.ino          ← Main entry point
│       ├── config.h                   ← Hardware pins, defaults, LEDC compat, theme colors
│       ├── nvs_storage.h              ← Non-volatile storage (WiFi, server config)
│       ├── wifi_manager.h             ← WiFi STA + SmartConfig fallback
│       ├── ugent_client.h             ← UGENT REST API client
│       ├── sse_client.h               ← SSE real-time event streaming
│       └── ui/
│           ├── ui_manager.h           ← Tab navigation and screen lifecycle
│           ├── ui_theme.h             ← Catppuccin Mocha dark theme + styles
│           ├── screen_dashboard.h     ← Main status display
│           ├── screen_tasks.h         ← Detailed task monitor
│           ├── screen_interact.h      ← Human-in-the-loop interaction
│           └── screen_settings.h      ← WiFi, server, brightness config
└── 2.8inch_ESP32-2432S028R/           ← Vendor docs, schematics, examples
```

## Features

| Feature | Description |
|---------|-------------|
| **Real-time Status** | Dashboard shows UGENT agent version, sessions, memory, cron jobs |
| **Task Monitor** | Live task list with status colors (green=running, blue=completed, red=failed) |
| **Human-in-the-Loop** | Approve or reject agent actions directly from the touchscreen |
| **Text Input** | On-screen keyboard for sending messages to the UGENT agent |
| **WiFi Setup** | Scan, select, and configure WiFi from the touchscreen (no PC needed) |
| **SmartConfig** | Fall back to ESP-Touch SmartConfig if no credentials stored |
| **SSE Streaming** | Real-time event updates (task changes, interaction requests, notifications) |
| **Dark Theme** | Catppuccin Mocha-inspired dark theme for comfortable viewing |
| **Auto-Dim** | Screen saver after 60 seconds of inactivity |
| **Brightness Control** | Adjustable display brightness via settings screen |

## Configuration

All settings are stored in ESP32 NVS (non-volatile storage) and configurable from the touchscreen:

| Setting | Default | Description |
|---------|---------|-------------|
| WiFi SSID | — | Your network name |
| WiFi Password | — | Your network password |
| UGENT Host | — | UGENT server IP address |
| UGENT Port | 8765 | UGENT server port |
| UGENT API Key | — | Authentication key |
| Brightness | 80% | Display brightness (0–100%) |

## UGENT Server Requirements

The firmware communicates with the UGENT **channel-web** plugin API:

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/v1/ugent/status` | GET | Full system status (sessions, tasks, memory, cron) |
| `/v1/ugent/events` | GET (SSE) | Real-time event stream |
| `/v1/ugent/commands` | POST | Send commands and interactions |

Ensure your UGENT server has the **channel-web** plugin enabled and is accessible from the ESP32's network.

## Troubleshooting

### Board Not Detected

- **Windows:** Check Device Manager > Ports. If no COM port appears, install the CP210x or CH9102F driver.
- **macOS:** Run `ls /dev/cu.*` in Terminal. Look for `cu.SLAB_USBtoUART` or `cu.usbserial-*`.
- Try a different USB cable (some cables are power-only).
- Try a different USB port.

### Upload Fails

- Hold the **BOOT** button on the board while clicking Upload, release when you see "Connecting..." progress.
- Reduce upload speed to `115200` in Tools > Upload Speed.
- Close Serial Monitor before uploading (the serial port cannot be used by both simultaneously).

### Build Errors

| Error | Fix |
|-------|-----|
| `lv_conf.h: No such file or directory` | Place `lv_conf.h` **next to** `lvgl/` in `libraries/`, NOT inside it. Use the pre-configured `firmware/lv_conf.h`. |
| `XPT2046_Touchscreen.h: No such file or directory` | Install XPT2046_Touchscreen by Paul Stoffregen (Library Manager, search "XPT2046") |
| `TFT_eSPI.h: No such file or directory` | Install TFT_eSPI library |
| `lvgl.h: No such file or directory` | Install LVGL library (version 8.x) |
| `ArduinoJson.h: No such file or directory` | Install ArduinoJson library |
| `'ledcSetup' was not declared` | You have ESP32 Core 3.x — the code auto-detects and uses `ledcAttach()` instead |
| `User_Setup.h` errors | Configure TFT_eSPI with correct pin definitions (see Step 4) |
| LVGL 9.x API errors | Downgrade LVGL to 8.x (completely different API) |
| Out of memory / SRAM | Use "Huge APP" partition scheme; reduce `LVGL_BUF_SIZE` in config.h |

### Touch Not Working

- The firmware uses **XPT2046_Touchscreen** library on a separate VSPI bus (display uses HSPI)
- Install from Library Manager: search "XPT2046" by Paul Stoffregen
- Touch pins: CLK=25, MOSI=32, MISO=39, CS=33, IRQ=36
- Auto-calibration in touch read callback — touch all corners to calibrate
- Touch reads are rate-limited to every 50ms to minimize blocking

### WiFi Won't Connect

- Ensure 2.4GHz network (ESP32 does not support 5GHz)
- Move closer to the router
- Use the Settings screen to re-enter credentials
- Check Serial Monitor for detailed error messages

### Display Shows Garbage / Wrong Colors

- **Grey vertical lines on some colors, others fine:** GPIO 12 is TFT MISO, NOT TFT_RST. If `TFT_RST` is set to 12, the ILI9341 reset signal gets corrupted. **Fix:** Set `#define TFT_RST -1` and `#define TFT_MISO 12` in User_Setup.h.
- **Completely black & white:** Missing `#define USE_HSPI_PORT`. The display is wired to HSPI pins (13/14/15/12). Without this flag, TFT_eSPI uses the wrong SPI bus. **Fix:** Add `#define USE_HSPI_PORT` to User_Setup.h.
- **3/4 screen flashing, 1/4 stable:** Same root cause — wrong SPI bus or wrong pin assignments. Use the exact User_Setup.h from `firmware/User_Setup.h`.
- **Touch not working with TFT_Touch:** TFT_Touch bit-bangs GPIO 33, which conflicts with TFT_eSPI SPI. **Fix:** Use XPT2046_Touchscreen library with separate VSPI (CLK=25, MOSI=32, MISO=39, CS=33).
- Verify `User_Setup.h` has `#define ILI9341_2_DRIVER`
- Make sure `LV_COLOR_16_SWAP` is `0` in `lv_conf.h`
- Try lowering `SPI_FREQUENCY` to `20000000` if still having issues

## Development Notes

- All source files use **header-only** pattern (`.h` files) for Arduino IDE compatibility
- LVGL 8.x API is used — do NOT migrate to LVGL 9.x without rewriting the UI layer
- The firmware uses a partial display buffer (`SCREEN_WIDTH × 10` pixels) to conserve RAM
- Colors use RGB565 format (16-bit) for LVGL compatibility
- Default theme is Catppuccin Mocha dark — customize colors in `config.h`
- **ESP32 Core 2.x and 3.x compatible** — `config.h` auto-detects the core version and uses the correct LEDC PWM API (`ledcSetup`/`ledcAttachPin` for 2.x, `ledcAttach` for 3.x)

## License

This project is part of the UGENT ecosystem. Refer to the main repository for license information.
