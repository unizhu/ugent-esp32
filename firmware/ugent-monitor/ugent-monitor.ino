/**
 * UGENT ESP32 Monitor — Main Entry Point
 *
 * Firmware for ESP32-2432S028R (2.8" TFT + Touch)
 *
 * PROGRESSIVE TEST VERSION:
 *   Shows 4 colored buttons (proves display works)
 *   Then adds UI elements one by one with serial output
 *   to find exactly where B&W starts.
 *
 * INIT ORDER: matches working test-lvgl exactly:
 *   lv_init() → touchSpi → tft.begin() → LVGL drivers → UI → WiFi
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

// ─── Helper: print LVGL memory ────────────────────────────────────────────────
static void print_mem(const char* label) {
    lv_mem_monitor_t mon;
    lv_mem_monitor(&mon);
    Serial.printf("[MEM] %s: total=%u free=%u used=%u (%u%%) frag=%u%% max_used=%u\n",
                  label, mon.total_size, mon.free_size,
                  mon.total_size - mon.free_size, mon.used_pct,
                  mon.frag_pct, mon.max_used);
}

// ─── Helper: render for N seconds ─────────────────────────────────────────────
static void render_for(int seconds, const char* label) {
    Serial.printf("[TEST] Showing '%s' for %d seconds...\n", label, seconds);
    for (int i = 0; i < seconds * 200; i++) {
        lv_timer_handler();
        delay(5);
    }
}

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

// ─── Setup ─────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 3000) { delay(10); }
    Serial.println("\n\n=== UGENT ESP32 Monitor (PROGRESSIVE TEST) ===");
    Serial.printf("[ Heap at start: %u bytes free ]\n", ESP.getFreeHeap());

    // ─── Init order matches working test-lvgl ─────────────────────────────
    lv_init();
    Serial.println("[STEP] lv_init() OK");

    touchSpi.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
    ts.begin(touchSpi);
    ts.setRotation(1);

    tft.begin();
    tft.setRotation(1);
    Serial.println("[STEP] TFT + Touch OK");

    // Draw buffer — heap allocated, 7680 pixels
    uint32_t buf_size = screenWidth * screenHeight / 10;
    buf = (lv_color_t*)malloc(buf_size * sizeof(lv_color_t));
    if (!buf) {
        Serial.println("[FATAL] draw buffer malloc failed");
        while (true) delay(1000);
    }
    Serial.printf("[STEP] Draw buf: %u px at %p\n", buf_size, buf);
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
    Serial.println("[STEP] LVGL drivers registered");

    print_mem("after drivers");

    // ─── Backlight ────────────────────────────────────────────────────────
    ugent_ledc_init(LCD_BL_PIN, BACKLIGHT_PWM_CHANNEL,
                    BACKLIGHT_PWM_FREQ, BACKLIGHT_PWM_RES);
    ugent_ledc_write(LCD_BL_PIN, BACKLIGHT_PWM_CHANNEL, 128);

    // ═══════════════════════════════════════════════════════════════════════
    // PROGRESSIVE TEST: add UI elements one at a time
    // ═══════════════════════════════════════════════════════════════════════

    // ─── TEST 1: Simple colored buttons (same as test-lvgl) ───────────────
    Serial.println("\n[TEST 1] 4 colored buttons (like test-lvgl)");
    {
        lv_obj_t *scr = lv_scr_act();
        lv_obj_set_style_bg_color(scr, lv_color_hex(0x1E1E2E), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);

        lv_obj_t *b1 = lv_btn_create(scr);
        lv_obj_set_size(b1, 140, 90);
        lv_obj_align(b1, LV_ALIGN_TOP_LEFT, 10, 10);
        lv_obj_set_style_bg_color(b1, lv_color_hex(0xFF0000), LV_PART_MAIN);
        lv_obj_t *l1 = lv_label_create(b1);
        lv_label_set_text(l1, "RED");
        lv_obj_center(l1);

        lv_obj_t *b2 = lv_btn_create(scr);
        lv_obj_set_size(b2, 140, 90);
        lv_obj_align(b2, LV_ALIGN_TOP_RIGHT, -10, 10);
        lv_obj_set_style_bg_color(b2, lv_color_hex(0x00FF00), LV_PART_MAIN);
        lv_obj_t *l2 = lv_label_create(b2);
        lv_label_set_text(l2, "GREEN");
        lv_obj_set_style_text_color(l2, lv_color_hex(0x000000), LV_PART_MAIN);
        lv_obj_center(l2);

        lv_obj_t *b3 = lv_btn_create(scr);
        lv_obj_set_size(b3, 140, 90);
        lv_obj_align(b3, LV_ALIGN_BOTTOM_LEFT, 10, -10);
        lv_obj_set_style_bg_color(b3, lv_color_hex(0x0000FF), LV_PART_MAIN);

        lv_obj_t *b4 = lv_btn_create(scr);
        lv_obj_set_size(b4, 140, 90);
        lv_obj_align(b4, LV_ALIGN_BOTTOM_RIGHT, -10, -10);
        lv_obj_set_style_bg_color(b4, lv_color_hex(0xFFFF00), LV_PART_MAIN);
    }
    print_mem("after test 1");
    render_for(3, "TEST 1: 4 colored buttons");
    lv_obj_clean(lv_scr_act());

    // ─── TEST 2: Apply theme + styles ─────────────────────────────────────
    Serial.println("\n[TEST 2] Theme styles on colored buttons");
    {
        theme_init();  // Creates 12 static styles
        print_mem("after theme_init()");

        lv_obj_t *scr = lv_scr_act();
        lv_obj_set_style_bg_color(scr, lv_color_hex(0x1E1E2E), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);

        // Button with theme styles
        lv_obj_t *card = lv_obj_create(scr);
        lv_obj_set_size(card, 300, 100);
        lv_obj_align(card, LV_ALIGN_TOP_MID, 0, 10);
        lv_obj_add_style(card, &style_card, 0);  // Surface0 bg, radius 8, padding 8

        lv_obj_t *title = lv_label_create(card);
        lv_label_set_text(title, "Card with style_card");
        lv_obj_add_style(title, &style_title, 0);  // Text color, font 16

        // Colored button with style
        lv_obj_t *btn = lv_btn_create(scr);
        lv_obj_set_size(btn, 200, 60);
        lv_obj_align(btn, LV_ALIGN_CENTER, 0, 20);
        lv_obj_add_style(btn, &style_btn_primary, 0);  // Blue bg, white text, font 14
        lv_obj_t *lbl = lv_label_create(btn);
        lv_label_set_text(lbl, "Primary Button");
        lv_obj_center(lbl);

        // Red status text
        lv_obj_t *status = lv_label_create(scr);
        lv_label_set_text(status, "Status: Running");
        lv_obj_add_style(status, &style_status_run, 0);  // Green text
        lv_obj_align(status, LV_ALIGN_BOTTOM_MID, 0, -20);
    }
    print_mem("after test 2");
    render_for(3, "TEST 2: themed buttons");
    lv_obj_clean(lv_scr_act());

    // ─── TEST 3: Tabview (no content) ─────────────────────────────────────
    Serial.println("\n[TEST 3] Tabview with empty tabs");
    {
        lv_obj_t *scr = lv_scr_act();
        lv_obj_set_style_bg_color(scr, lv_color_hex(0x1E1E2E), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);

        lv_obj_t *tv = lv_tabview_create(scr, LV_DIR_BOTTOM, 36);
        lv_obj_set_size(tv, SCREEN_WIDTH, SCREEN_HEIGHT);
        lv_obj_set_style_bg_color(tv, lv_color_hex(0x1E1E2E), LV_PART_MAIN);

        // Style tab buttons
        lv_obj_t* tab_btns = lv_tabview_get_tab_btns(tv);
        lv_obj_set_style_bg_color(tab_btns, lv_color_hex(0x313244), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(tab_btns, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_text_color(tab_btns, lv_color_hex(0xCDD6F4), LV_PART_ITEMS);
        lv_obj_set_style_text_font(tab_btns, &lv_font_montserrat_10, LV_PART_ITEMS);

        lv_obj_t *t1 = lv_tabview_add_tab(tv, "Home");
        lv_obj_t *t2 = lv_tabview_add_tab(tv, "Tasks");
        lv_obj_t *t3 = lv_tabview_add_tab(tv, "Chat");
        lv_obj_t *t4 = lv_tabview_add_tab(tv, "Setup");

        // Set bg for each tab
        lv_obj_t* tabs[] = {t1, t2, t3, t4};
        for (int i = 0; i < 4; i++) {
            lv_obj_set_style_bg_color(tabs[i], lv_color_hex(0x1E1E2E), LV_PART_MAIN);
            lv_obj_set_style_bg_opa(tabs[i], LV_OPA_COVER, LV_PART_MAIN);
            lv_obj_clear_flag(tabs[i], LV_OBJ_FLAG_SCROLLABLE);
        }

        // Add a colored label to tab 1
        lv_obj_t *lbl = lv_label_create(t1);
        lv_label_set_text(lbl, "TABVIEW TEST");
        lv_obj_set_style_text_color(lbl, lv_color_hex(0x89B4FA), LV_PART_MAIN);
        lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);
    }
    print_mem("after test 3");
    render_for(3, "TEST 3: tabview");
    lv_obj_clean(lv_scr_act());

    // ─── TEST 4: Dashboard screen (most objects) ─────────────────────────
    Serial.println("\n[TEST 4] Dashboard screen only");
    {
        lv_obj_t *scr = lv_scr_act();
        lv_obj_set_style_bg_color(scr, lv_color_hex(0x1E1E2E), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);

        screen_dashboard_create(scr);
    }
    print_mem("after test 4");
    render_for(3, "TEST 4: dashboard");
    lv_obj_clean(lv_scr_act());

    // ─── TEST 5: Full UI (tabview + all screens) ─────────────────────────
    Serial.println("\n[TEST 5] Full UI (ui.begin())");
    ui.setClients(&ugent, &wifi, &nvs);
    ui.begin();
    print_mem("after ui.begin()");

    // If we got here without crash, run normally
    Serial.printf("[ Heap before WiFi: %u bytes free ]\n", ESP.getFreeHeap());

    // ─── WiFi / NVS / SSE ─────────────────────────────────────────────────
    init_connectivity();

    interact_history_init();

    currentBrightness = nvs.getBrightness();
    if (currentBrightness < 10) currentBrightness = 128;
    ugent_ledc_write(LCD_BL_PIN, BACKLIGHT_PWM_CHANNEL, currentBrightness);

    print_mem("after full init");
    Serial.printf("[ Heap final: %u bytes free ]\n", ESP.getFreeHeap());
    Serial.println("[UGENT] Ready! Looping...");
}

// ─── Connectivity ──────────────────────────────────────────────────────────────

static void init_connectivity() {
    if (!nvs.begin()) {
        Serial.println("[UGENT] NVS init failed");
        return;
    }

    wifi.begin(&nvs);

    if (nvs.hasWifiCredentials()) {
        String ssid = nvs.getWifiSsid();
        String pass = nvs.getWifiPass();
        Serial.printf("[UGENT] WiFi: connecting to %s\n", ssid.c_str());

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
        Serial.println("[UGENT] SSE started");
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
