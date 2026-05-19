/**
 * UGENT ESP32 Monitor — Main Entry Point
 *
 * Firmware for ESP32-2432S028R (2.8" TFT + Touch)
 * Displays UGENT agent status, tasks, and enables human-in-the-loop interaction.
 *
 * DISPLAY & COLOR: matches vendor LVGL_Arduino.ino example exactly:
 *   - ILI9341_2_DRIVER in User_Setup.h (vendor's exact file)
 *   - LV_COLOR_16_SWAP = 0 in lv_conf.h
 *   - pushColors(..., true) in flush callback
 *   - setRotation(1) for landscape 320x240
 *
 * TOUCH: matches vendor LVGL_Arduino.ino example exactly:
 *   - TFT_Touch library with bit-banged GPIO on pins 33,25,32,39
 *   - touch.setCal(526, 3443, 750, 3377, 320, 240, 1)
 *   - Rate-limited to every 50ms to minimize blocking
 */

#include <Arduino.h>
#include <lvgl.h>
#include <TFT_eSPI.h>
#include <TFT_Touch.h>
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

// ─── Globals ───────────────────────────────────────────────────────────────────

static TFT_eSPI tft = TFT_eSPI(screenWidth, screenHeight);

// Touch — same pins and library as vendor LVGL example
static TFT_Touch touch = TFT_Touch(33, 25, 32, 39);  // CS, CLK, DIN, DOUT

static NvsStorage  nvs;
static WifiManager wifi;
static UgentClient ugent;
static SseClient   sse;
static UIManager   ui;

// LVGL display driver — heap-allocated buffer to avoid DRAM overflow
// (320*240/4 = 38KB static BSS overflows DRAM with WiFi/SSE/NVS modules)
static lv_disp_draw_buf_t draw_buf;
static lv_color_t* buf = nullptr;
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

// ─── Touch Read (TFT_Touch library, rate-limited) ───────────────────────────────
// Same as vendor LVGL_Arduino.ino but with 50ms rate limiting.
// TFT_Touch uses bit-banged GPIO with delay(1) calls (~20ms per read).
// Rate limiting prevents it from blocking the LVGL render loop.

void my_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data) {
    unsigned long now = millis();

    // Rate limit: only actually read hardware every 50ms
    if (now - lastTouchRead >= 50) {
        lastTouchRead = now;
        bool touched = touch.Pressed();

        if (!touched) {
            lastTouched = false;
        } else {
            lastTouchX = touch.X();
            lastTouchY = touch.Y();
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

    // Touch init — exact same as vendor LVGL example
    touch.setCal(526, 3443, 750, 3377, 320, 240, 1);

    Serial.println("[UGENT] TFT + Touch initialized (vendor config)");

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

    // Heap-allocated draw buffer — avoids DRAM BSS overflow
    // screenWidth * 10 = 6400 bytes (plenty for LVGL partial rendering)
    uint32_t buf_size = screenWidth * 10;
    buf = (lv_color_t*)malloc(buf_size * sizeof(lv_color_t));
    if (!buf) {
        Serial.println("[UGENT] FAIL: LVGL draw buffer alloc");
        return false;
    }
    lv_disp_draw_buf_init(&draw_buf, buf, NULL, buf_size);

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
