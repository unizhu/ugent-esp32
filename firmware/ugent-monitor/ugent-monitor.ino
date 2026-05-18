/**
 * UGENT ESP32 Monitor — Main Entry Point
 *
 * Firmware for ESP32-2432S028R (2.8" TFT + Touch)
 * Displays UGENT agent status, tasks, and enables human-in-the-loop interaction.
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
static TFT_Touch   touch(TOUCH_CS_PIN, TOUCH_IRQ_PIN);
static NvsStorage  nvs;
static WifiManager wifi;
static UgentClient ugent;
static SSEClient   sse;
static UIManager   ui;

// LVGL display/touch driver buffers
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf1[SCREEN_WIDTH * 10];
static lv_indev_drv_t indev_drv;

// Backlight PWM
static uint8_t currentBrightness = 128;

// ─── Display Flush ─────────────────────────────────────────────────────────────

static void disp_flush(lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* color_p) {
    uint32_t w = area->x2 - area->x1 + 1;
    uint32_t h = area->y2 - area->y1 + 1;
    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors((uint16_t*)color_p, w * h, true);
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

    uint16_t x, y;
    touch.ReadRaw();
    touch.Read();
    x = touch.X();
    y = touch.Y();

    // Calibration and bounds check
    if (x > SCREEN_WIDTH) x = SCREEN_WIDTH;
    if (y > SCREEN_HEIGHT) y = SCREEN_HEIGHT;

    data->point.x = x;
    data->point.y = y;
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

    // Touch init with calibration
    touch.begin();
    touch.setCal(TOUCH_CAL_0, TOUCH_CAL_1, TOUCH_CAL_2, TOUCH_CAL_3,
                 SCREEN_WIDTH, SCREEN_HEIGHT, 1);

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
    lv_disp_draw_buf_init(&draw_buf, buf1, nullptr, SCREEN_WIDTH * 10);

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
