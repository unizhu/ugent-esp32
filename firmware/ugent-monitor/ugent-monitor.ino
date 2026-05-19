/**
 * UGENT ESP32 Monitor — Main Entry Point
 *
 * Firmware for ESP32-2432S028R (2.8" TFT + Touch)
 * Displays UGENT agent status, tasks, and enables human-in-the-loop interaction.
 *
 * DISPLAY: matches witnessmenow/ESP32-Cheap-Yellow-Display proven config:
 *   - ILI9341_2_DRIVER in User_Setup.h
 *   - USE_HSPI_PORT — display on HSPI (SPI2), MISO=GPIO12
 *   - TFT_RST -1 — reset tied to board reset, NOT GPIO 12
 *   - LV_COLOR_16_SWAP = 0 in lv_conf.h
 *   - pushColors(..., true) in flush callback
 *   - setRotation(1) for landscape 320x240
 *
 * TOUCH: separate VSPI bus via XPT2046_Touchscreen library:
 *   - CLK=25, MOSI=32, MISO=39, CS=33, IRQ=36
 *   - Rate-limited to every 50ms to minimize blocking
 */

#include <Arduino.h>
#include <lvgl.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <SPI.h>
#include <WiFi.h>

#include "config.h"
#include "nvs_storage.h"
#include "wifi_manager.h"
#include "ugent_client.h"
#include "sse_client.h"
#include "ui/ui_manager.h"

// ─── Screen ────────────────────────────────────────────────────────────────────
static const uint16_t screenWidth  = 320;
static const uint16_t screenHeight = 240;

// ─── Touch pins (separate VSPI bus) ───────────────────────────────────────────
#define XPT2046_CLK  25
#define XPT2046_MISO 39
#define XPT2046_MOSI 32
#define XPT2046_CS   33
#define XPT2046_IRQ  36

// ─── Globals ───────────────────────────────────────────────────────────────────

static TFT_eSPI tft = TFT_eSPI();  // Default ctor uses TFT_WIDTH/TFT_HEIGHT (portrait) from User_Setup.h

// Touch — separate VSPI instance (display uses HSPI)
static SPIClass touchSpi = SPIClass(VSPI);
static XPT2046_Touchscreen ts(XPT2046_CS, XPT2046_IRQ);

// Touch calibration range (from witnessmenow example)
static uint16_t touchMinX = 200, touchMaxX = 3700;
static uint16_t touchMinY = 240,  touchMaxY = 3800;

static NvsStorage  nvs;
static WifiManager wifi;
static UgentClient ugent;
static SseClient   sse;
static UIManager   ui;

// LVGL display driver — witnessmenow pattern: static partial buffer
// screenWidth * screenHeight / 10 = 7680 pixels (15KB)
// Larger full-frame buffers (screenWidth*screenHeight/4 = 38KB) may overflow
// DRAM BSS when WiFi/SSE/NVS modules are linked.
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[screenWidth * screenHeight / 10];
static lv_indev_drv_t indev_drv;

// Touch rate limiting — only poll every 50ms to minimize blocking
static unsigned long lastTouchRead = 0;
static bool lastTouched = false;
static uint16_t lastTouchX = 0;
static uint16_t lastTouchY = 0;

// Backlight PWM
static uint8_t currentBrightness = 128;
// ─── Display Flush ─────────────────────────────────────────────────────────────
// EXACT copy from vendor LVGL_Arduino.ino example

void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors((uint16_t *)&color_p->full, w * h, true);
    tft.endWrite();

    lv_disp_flush_ready(disp);
}

// ─── Touch Read (XPT2046_Touchscreen on separate VSPI, rate-limited) ───────
// Touch uses a separate VSPI bus from the display (HSPI), no SPI conflict.
// Rate-limited to every 50ms.

void my_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data) {
    unsigned long now = millis();

    // Rate limit: only actually read hardware every 50ms
    if (now - lastTouchRead >= 50) {
        lastTouchRead = now;
        bool touched = ts.touched();

        if (!touched) {
            lastTouched = false;
        } else {
            TS_Point p = ts.getPoint();
            // Auto-calibrate min/max
            if (p.x < touchMinX) touchMinX = p.x;
            if (p.x > touchMaxX) touchMaxX = p.x;
            if (p.y < touchMinY) touchMinY = p.y;
            if (p.y > touchMaxY) touchMaxY = p.y;
            // Map to screen coordinates
            lastTouchX = map(p.x, touchMinX, touchMaxX, 0, screenWidth);
            lastTouchY = map(p.y, touchMinY, touchMaxY, 0, screenHeight);
            lastTouchX = constrain(lastTouchX, 0, screenWidth - 1);
            lastTouchY = constrain(lastTouchY, 0, screenHeight - 1);
            lastTouched = true;
        }
    }

    if (!lastTouched) {
        data->state = LV_INDEV_STATE_REL;
    } else {
        data->state = LV_INDEV_STATE_PR;
        data->point.x = lastTouchX;
        data->point.y = lastTouchY;
    }
}// ─── Hardware Init ─────────────────────────────────────────────────────────────

static bool init_hardware() {
    Serial.begin(115200);
    while (!Serial && millis() < 3000) { delay(10); }
    Serial.println("[UGENT] Boot");

    // TFT init — same as vendor example
    tft.begin();
    tft.setRotation(1);  // Landscape orientation
    tft.fillScreen(TFT_BLACK);

    // Touch init — separate VSPI bus (display uses HSPI)
    touchSpi.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
    ts.begin(touchSpi);
    ts.setRotation(1);  // Landscape

    Serial.println("[UGENT] TFT + Touch initialized (witnessmenow config)");

    // Backlight PWM (compatible with ESP32 Core 2.x and 3.x)
    ugent_ledc_init(LCD_BL_PIN, BACKLIGHT_PWM_CHANNEL,
                    BACKLIGHT_PWM_FREQ, BACKLIGHT_PWM_RES);
    currentBrightness = nvs.getBrightness();
    if (currentBrightness < 10) currentBrightness = 128;
    ugent_ledc_write(LCD_BL_PIN, BACKLIGHT_PWM_CHANNEL, currentBrightness);

    Serial.println("[UGENT] Hardware OK");
    return true;
}

// ─── LVGL Init ─────────────────────────────────────────────────────────────────
// Matches vendor LVGL_Arduino.ino setup exactly

static bool init_lvgl() {
    lv_init();

    // Static draw buffer — witnessmenow pattern
    // Using partial buffer (7680 pixels) instead of full frame (76800 pixels)
    lv_disp_draw_buf_init(&draw_buf, buf, NULL, screenWidth * screenHeight / 10);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = screenWidth;
    disp_drv.ver_res = screenHeight;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touchpad_read;
    lv_indev_drv_register(&indev_drv);

    Serial.println("[UGENT] LVGL OK");
    return true;
}

// ─── Backlight Update (from settings) ─────────────────────────────────────────

static void update_backlight() {
    uint8_t target = nvs.getBrightness();
    if (target != currentBrightness && target >= 10) {
        currentBrightness = target;
        ugent_ledc_write(LCD_BL_PIN, BACKLIGHT_PWM_CHANNEL, currentBrightness);
    }
}

// ─── SSE Event Callback ────────────────────────────────────────────────────────

static void on_sse_event(const SseEvent& event) {
    switch (event.type) {
        case SseEventType::TASK_STATUS_CHANGED:
            if (ugent.fetchStatus()) {
                ui.updateDashboard(&ugent);
                ui.updateTasks(&ugent);
            }
            break;
        case SseEventType::INTERACTION_REQUEST:
            ui.showInteraction(event.data);
            break;
        case SseEventType::CHAT_NOTIFICATION:
            ui.showAlert(event.data);
            break;
        default:
            break;
    }
}

// ─── Connectivity ──────────────────────────────────────────────────────────────

static bool init_connectivity() {
    if (!nvs.begin()) {
        Serial.println("[UGENT] NVS init failed");
        return false;
    }

    wifi.begin(&nvs);

    if (nvs.hasWifiCredentials()) {
        String ssid = nvs.getWifiSsid();
        String pass = nvs.getWifiPass();
        Serial.printf("[UGENT] WiFi: connecting to %s\n", ssid.c_str());

        if (wifi.connect(ssid.c_str(), pass.c_str())) {
            Serial.printf("[UGENT] WiFi: IP %s\n", WiFi.localIP().toString().c_str());
        } else {
            Serial.println("[UGENT] WiFi: failed, showing settings");
        }
    } else {
        Serial.println("[UGENT] No WiFi credentials");
    }

    ugent.begin(&nvs);
    Serial.printf("[UGENT] Server: %s:%d\n",
        nvs.getUgentHost().c_str(), nvs.getUgentPort());

    if (wifi.isConnected()) {
        sse.begin(&nvs);
        sse.onEvent(on_sse_event);
        Serial.println("[UGENT] SSE started");
    }

    return true;
}

// ─── Setup ─────────────────────────────────────────────────────────────────────

void setup() {
    Serial.println("=== UGENT ESP32 Monitor ===");

    if (!init_hardware()) {
        Serial.println("[UGENT] HAL init FAILED");
        tft.fillScreen(TFT_RED);
        tft.setTextColor(TFT_WHITE);
        tft.drawString("HW ERROR", 100, 100, 2);
        while (true) delay(1000);
    }

    if (!init_lvgl()) {
        Serial.println("[UGENT] LVGL init FAILED");
        tft.fillScreen(TFT_RED);
        tft.setTextColor(TFT_WHITE);
        tft.drawString("LVGL ERROR", 80, 100, 2);
        while (true) delay(1000);
    }

    init_connectivity();

    ui.setClients(&ugent, &wifi, &nvs);
    ui.begin();
    interact_history_init();

    if (wifi.isConnected() && ugent.testConnection()) {
        ugent.fetchStatus();
        ui.updateDashboard(&ugent);
        ui.updateTasks(&ugent);
    }

    Serial.println("[UGENT] Ready!");
}

// ─── Loop ──────────────────────────────────────────────────────────────────────

static unsigned long lastStatusPoll = 0;
static unsigned long lastBacklightCheck = 0;

void loop() {
    lv_timer_handler();
    delay(5);  // Same as vendor example

    unsigned long now = millis();

    if (wifi.isConnected() && ugent.isConnected() &&
        (now - lastStatusPoll > STATUS_POLL_INTERVAL_MS)) {
        lastStatusPoll = now;
        if (ugent.fetchStatus()) {
            ui.updateDashboard(&ugent);
            ui.updateTasks(&ugent);
        }
    }

    sse.loop();

    if (now - lastBacklightCheck > 1000) {
        lastBacklightCheck = now;
        update_backlight();
    }
}
