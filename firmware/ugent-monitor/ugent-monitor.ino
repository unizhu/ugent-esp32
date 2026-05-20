/**
 * UGENT ESP32 Monitor — Main Entry Point
 *
 * Firmware for ESP32-2432S028R (2.8" TFT + Touch)
 * Displays UGENT agent status, tasks, and enables human-in-the-loop interaction.
 *
 * INIT ORDER: matches working test-lvgl exactly:
 *   lv_init() → touchSpi → tft.begin() → LVGL drivers → UI → WiFi
 *
 * THEME: Vivid palette for 16-bit TFT visibility.
 *        Catppuccin Mocha was too muted/washed out on 2.8" display.
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

TFT_eSPI tft = TFT_eSPI(screenWidth, screenHeight);

static SPIClass touchSpi = SPIClass(VSPI);
static XPT2046_Touchscreen ts(XPT2046_CS, XPT2046_IRQ);

static uint16_t touchMinX = 200, touchMaxX = 3700;
static uint16_t touchMinY = 240,  touchMaxY = 3800;

static NvsStorage  nvs;
static WifiManager wifi;
static UgentClient ugent;
static SseClient   sse;
static UIManager   ui;

static lv_disp_draw_buf_t draw_buf;
static lv_color_t* buf = nullptr;
static lv_indev_drv_t indev_drv;

static unsigned long lastTouchRead = 0;
static bool lastTouched = false;
static uint16_t lastTouchX = 0;
static uint16_t lastTouchY = 0;

static uint8_t currentBrightness = 128;

// ─── Display Flush ─────────────────────────────────────────────────────────────
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors((uint16_t *)&color_p->full, w * h, true);
    tft.endWrite();
    lv_disp_flush_ready(disp);
}

// ─── Touch Read ────────────────────────────────────────────────────────────────
void my_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data) {
    unsigned long now = millis();
    if (now - lastTouchRead >= 50) {
        lastTouchRead = now;
        bool touched = ts.touched();
        if (!touched) {
            lastTouched = false;
        } else {
            TS_Point p = ts.getPoint();
            if (p.x < touchMinX) touchMinX = p.x;
            if (p.x > touchMaxX) touchMaxX = p.x;
            if (p.y < touchMinY) touchMinY = p.y;
            if (p.y > touchMaxY) touchMaxY = p.y;
            lastTouchX = constrain(map(p.x, touchMinX, touchMaxX, 0, screenWidth), 0, screenWidth - 1);
            lastTouchY = constrain(map(p.y, touchMinY, touchMaxY, 0, screenHeight), 0, screenHeight - 1);
            lastTouched = true;
        }
    }
    if (lastTouched) {
        data->state = LV_INDEV_STATE_PR;
        data->point.x = lastTouchX;
        data->point.y = lastTouchY;
    } else {
        data->state = LV_INDEV_STATE_REL;
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

// ─── Setup ─────────────────────────────────────────────────────────────────────

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 3000) { delay(10); }
    Serial.println("\n\n\n****************************************************");
    Serial.println("*  UGENT ESP32 Monitor - BOOT                     *");
    Serial.println("****************************************************");
    Serial.printf("[Heap start: %u]\n", ESP.getFreeHeap());

    // Init order matches working test-lvgl EXACTLY
    lv_init();

    touchSpi.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
    ts.begin(touchSpi);
    ts.setRotation(1);

    tft.begin();
    tft.setRotation(1);
    Serial.println("[UGENT] TFT + Touch OK");

    // Heap draw buffer — 7680 pixels (same size as working test-lvgl)
    uint32_t buf_size = screenWidth * screenHeight / 10;
    buf = (lv_color_t*)malloc(buf_size * sizeof(lv_color_t));
    if (!buf) {
        Serial.println("[FATAL] draw buffer malloc failed");
        while (true) delay(1000);
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
    Serial.println("[UGENT] LVGL drivers OK");

    // Backlight
    ugent_ledc_init(LCD_BL_PIN, BACKLIGHT_PWM_CHANNEL,
                    BACKLIGHT_PWM_FREQ, BACKLIGHT_PWM_RES);
    ugent_ledc_write(LCD_BL_PIN, BACKLIGHT_PWM_CHANNEL, 128);

    // Quick color verification — 4 unmistakable primary colors
    {
        lv_obj_t *scr = lv_scr_act();
        lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);

        lv_obj_t *b1 = lv_btn_create(scr);
        lv_obj_set_size(b1, 150, 115);
        lv_obj_align(b1, LV_ALIGN_TOP_LEFT, 5, 5);
        lv_obj_set_style_bg_color(b1, lv_color_hex(0xFF0000), LV_PART_MAIN);  // PURE RED
        lv_obj_t *l1 = lv_label_create(b1);
        lv_label_set_text(l1, "RED");
        lv_obj_set_style_text_color(l1, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        lv_obj_center(l1);

        lv_obj_t *b2 = lv_btn_create(scr);
        lv_obj_set_size(b2, 150, 115);
        lv_obj_align(b2, LV_ALIGN_TOP_RIGHT, -5, 5);
        lv_obj_set_style_bg_color(b2, lv_color_hex(0x00FF00), LV_PART_MAIN);  // PURE GREEN
        lv_obj_t *l2 = lv_label_create(b2);
        lv_label_set_text(l2, "GREEN");
        lv_obj_set_style_text_color(l2, lv_color_hex(0x000000), LV_PART_MAIN);
        lv_obj_center(l2);

        lv_obj_t *b3 = lv_btn_create(scr);
        lv_obj_set_size(b3, 150, 115);
        lv_obj_align(b3, LV_ALIGN_BOTTOM_LEFT, 5, -5);
        lv_obj_set_style_bg_color(b3, lv_color_hex(0x0000FF), LV_PART_MAIN);  // PURE BLUE
        lv_obj_t *l3 = lv_label_create(b3);
        lv_label_set_text(l3, "BLUE");
        lv_obj_set_style_text_color(l3, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        lv_obj_center(l3);

        lv_obj_t *b4 = lv_btn_create(scr);
        lv_obj_set_size(b4, 150, 115);
        lv_obj_align(b4, LV_ALIGN_BOTTOM_RIGHT, -5, -5);
        lv_obj_set_style_bg_color(b4, lv_color_hex(0xFFFF00), LV_PART_MAIN);  // PURE YELLOW
        lv_obj_t *l4 = lv_label_create(b4);
        lv_label_set_text(l4, "YELLOW");
        lv_obj_set_style_text_color(l4, lv_color_hex(0x000000), LV_PART_MAIN);
        lv_obj_center(l4);
    }
    Serial.println("[UGENT] Showing 4 PRIMARY colors (RED/GREEN/BLUE/YELLOW) for 3 sec");
    for (int i = 0; i < 600; i++) { lv_timer_handler(); delay(5); }
    lv_obj_clean(lv_scr_act());
    Serial.println("[UGENT] Color test done");

    // Real UI
    ui.setClients(&ugent, &wifi, &nvs);
    ui.begin();
    Serial.println("[UGENT] UI created");

    // LVGL memory check
    lv_mem_monitor_t mon;
    lv_mem_monitor(&mon);
    Serial.printf("[UGENT] LVGL mem: %u/%u used (%u%%) frag=%u%%\n",
                  mon.total_size - mon.free_size, mon.total_size,
                  mon.used_pct, mon.frag_pct);

    // WiFi/NVS/SSE — after display is confirmed working
    if (!nvs.begin()) {
        Serial.println("[UGENT] NVS init failed");
    } else {
        wifi.begin(&nvs);
        if (nvs.hasWifiCredentials()) {
            String ssid = nvs.getWifiSsid();
            String pass = nvs.getWifiPass();
            Serial.printf("[UGENT] WiFi: %s\n", ssid.c_str());
            if (wifi.connect(ssid.c_str(), pass.c_str())) {
                Serial.printf("[UGENT] WiFi: IP %s\n", WiFi.localIP().toString().c_str());
            } else {
                Serial.println("[UGENT] WiFi: failed");
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
        }

        currentBrightness = nvs.getBrightness();
        if (currentBrightness < 10) currentBrightness = 128;
        ugent_ledc_write(LCD_BL_PIN, BACKLIGHT_PWM_CHANNEL, currentBrightness);
    }

    interact_history_init();

    if (wifi.isConnected() && ugent.testConnection()) {
        ugent.fetchStatus();
        ui.updateDashboard(&ugent);
        ui.updateTasks(&ugent);
    }

    Serial.printf("[Heap final: %u]\n", ESP.getFreeHeap());
    Serial.println("[UGENT] Ready!");
}

// ─── Loop ──────────────────────────────────────────────────────────────────────

static unsigned long lastStatusPoll = 0;
static unsigned long lastBacklightCheck = 0;

void loop() {
    lv_timer_handler();
    delay(5);

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
        uint8_t target = nvs.getBrightness();
        if (target != currentBrightness && target >= 10) {
            currentBrightness = target;
            ugent_ledc_write(LCD_BL_PIN, BACKLIGHT_PWM_CHANNEL, currentBrightness);
        }
    }
}
