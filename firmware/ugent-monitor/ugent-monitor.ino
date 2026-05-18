/**
 * UGENT ESP32 Monitor — Main Entry Point
 *
 * Firmware for ESP32-2432S028R (2.8" TFT + Touch)
 * Displays UGENT agent status, tasks, and enables human-in-the-loop interaction.
 *
 * HARDWARE NOTES:
 *   Display (ILI9341) and Touch (XPT2046) are on SEPARATE SPI buses.
 *   - Display SPI: MOSI=13, SCLK=14, CS=15, DC=2, RST=12
 *   - Touch SPI:   DOUT(MISO)=39, DIN(MOSI)=32, CS=33, CLK=25
 *   We use TFT_eSPI for display and TFT_Touch for touch input.
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

// ─── Globals ───────────────────────────────────────────────────────────────────

static TFT_eSPI    tft;
static NvsStorage  nvs;
static WifiManager wifi;
static UgentClient ugent;
static SseClient   sse;
static UIManager   ui;

// LVGL display/touch driver buffers
static lv_disp_draw_buf_t draw_buf;
static lv_color_t* buf1 = nullptr;
static lv_color_t* buf2 = nullptr;  // Double buffer for flicker-free rendering
static lv_indev_drv_t indev_drv;

// Touch controller on SEPARATE SPI bus (not shared with display)
// Pins: CS=33, CLK=25, DIN(MOSI)=32, DOUT(MISO)=39
static TFT_Touch touch(TOUCH_CS, TOUCH_CLK, TOUCH_DIN, TOUCH_DOUT);

// Backlight PWM
static uint8_t currentBrightness = 128;

// ─── Display Flush ─────────────────────────────────────────────────────────────

static void disp_flush(lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* color_p) {
    uint32_t w = area->x2 - area->x1 + 1;
    uint32_t h = area->y2 - area->y1 + 1;
    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    // Use false for byteSwap — LV_COLOR_16_SWAP handles it in lv_conf.h
    tft.pushColors((uint16_t*)color_p, w * h, false);
    tft.endWrite();
    lv_disp_flush_ready(drv);
}

// ─── Touch Read ─────────────────────────────────────────────────────────────────

static void touch_read(lv_indev_drv_t* drv, lv_indev_data_t* data) {
    bool touched = touch.Pressed();
    if (!touched) {
        data->state = LV_INDEV_STATE_REL;
        return;
    }

    uint16_t touchX = touch.X();
    uint16_t touchY = touch.Y();

    // Bounds check
    if (touchX > SCREEN_WIDTH - 1) touchX = SCREEN_WIDTH - 1;
    if (touchY > SCREEN_HEIGHT - 1) touchY = SCREEN_HEIGHT - 1;

    data->point.x = touchX;
    data->point.y = touchY;
    data->state = LV_INDEV_STATE_PR;
}

// ─── Hardware Init ─────────────────────────────────────────────────────────────

static bool init_hardware() {
    Serial.begin(115200);
    while (!Serial && millis() < 3000) { delay(10); }
    Serial.println("[UGENT] Boot");

    // TFT init
    tft.begin();
    tft.setRotation(ROTATION);
    tft.fillScreen(TFT_BLACK);

    // Touch init using TFT_Touch library with calibration from vendor example
    // setCal(xMin, xMax, yMin, yMax, xRes, yRes, axisSwap)
    touch.setCal(TOUCH_CAL_XMIN, TOUCH_CAL_XMAX, TOUCH_CAL_YMIN, TOUCH_CAL_YMAX,
                 SCREEN_WIDTH, SCREEN_HEIGHT, TOUCH_AXIS_SWAP);

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

static bool init_lvgl() {
    lv_init();

    // Double-buffered draw buffer for flicker-free rendering
    // Use SCREEN_WIDTH * 10 lines per buffer
    uint32_t buf_size = SCREEN_WIDTH * 10;
    buf1 = (lv_color_t*)malloc(buf_size * sizeof(lv_color_t));
    buf2 = (lv_color_t*)malloc(buf_size * sizeof(lv_color_t));
    if (!buf1) {
        Serial.println("[UGENT] FAIL: LVGL draw buf1 alloc");
        return false;
    }
    if (!buf2) {
        Serial.println("[UGENT] WARN: No buf2, single-buffer mode");
        lv_disp_draw_buf_init(&draw_buf, buf1, nullptr, buf_size);
    } else {
        lv_disp_draw_buf_init(&draw_buf, buf1, buf2, buf_size);
    }

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = SCREEN_WIDTH;
    disp_drv.ver_res = SCREEN_HEIGHT;
    disp_drv.flush_cb = disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touch_read;
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
            // Re-fetch full status via HTTP for consistency
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
    // NVS
    if (!nvs.begin()) {
        Serial.println("[UGENT] NVS init failed");
        return false;
    }

    // WiFi
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

    // UGENT client
    ugent.begin(&nvs);
    Serial.printf("[UGENT] Server: %s:%d\n",
        nvs.getUgentHost().c_str(), nvs.getUgentPort());

    // SSE client
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

    // Init connectivity (NVS, WiFi, UGENT client, SSE)
    init_connectivity();

    // Init UI — set clients BEFORE begin so begin() can wire up callbacks
    ui.setClients(&ugent, &wifi, &nvs);
    ui.begin();
    interact_history_init();  // Allocate message history on heap

    // Initial status fetch
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
    // LVGL task handler — must be called frequently
    lv_timer_handler();
    delay(2);

    unsigned long now = millis();

    // Poll UGENT status periodically
    if (wifi.isConnected() && ugent.isConnected() &&
        (now - lastStatusPoll > STATUS_POLL_INTERVAL_MS)) {
        lastStatusPoll = now;
        if (ugent.fetchStatus()) {
            ui.updateDashboard(&ugent);
            ui.updateTasks(&ugent);
        }
    }

    // SSE loop (handles reconnection internally)
    sse.loop();

    // Backlight update from settings
    if (now - lastBacklightCheck > 1000) {
        lastBacklightCheck = now;
        update_backlight();
    }
}
