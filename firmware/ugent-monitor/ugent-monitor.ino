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

// Same constructor as working test-lvgl and vendor demo
TFT_eSPI tft = TFT_eSPI(screenWidth, screenHeight);

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

// LVGL display driver — heap-allocated draw buffer (avoids DRAM0 BSS overflow)
// test-lvgl uses static buf[7680] but ugent-monitor links WiFi/NVS/SSE
// which adds ~40KB BSS. Heap allocation: malloc returns 8-byte aligned DMA-capable DRAM.
// Buffer size matches test-lvgl: screenWidth * screenHeight / 10 = 7680 pixels = 15.36KB
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
// EXACT copy from vendor LVGL_Arduino.ino example and working test-lvgl

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
}

// ─── LVGL + Hardware Init ──────────────────────────────────────────────────────
// Matches working test-lvgl init order EXACTLY:
//   1. Serial
//   2. lv_init()
//   3. touchSpi.begin() + ts.begin()
//   4. tft.begin() + tft.setRotation(1)
//   5. lv_disp_draw_buf_init
//   6. Register display driver
//   7. Register input driver
//   Then: backlight, WiFi, UI

static bool init_system() {
    Serial.begin(115200);
    while (!Serial && millis() < 3000) { delay(10); }
    Serial.println("=== UGENT ESP32 Monitor ===");

    // ─── Step 1: lv_init() FIRST — matches test-lvgl order ────────────────
    lv_init();
    Serial.println("[UGENT] lv_init() done");

    // ─── Step 2: Touch init (separate VSPI bus) ──────────────────────────
    touchSpi.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
    ts.begin(touchSpi);
    ts.setRotation(1);

    // ─── Step 3: TFT init — NO fillScreen (matches test-lvgl) ────────────
    tft.begin();
    tft.setRotation(1);
    // Do NOT call tft.fillScreen() here — test-lvgl doesn't do this
    Serial.println("[UGENT] TFT + Touch initialized");

    // ─── Step 4: LVGL draw buffer — heap allocated, same size as test-lvgl ─
    // test-lvgl uses static buf[7680]. We use heap to avoid DRAM0 overflow.
    // 7680 pixels * 2 bytes = 15.36KB on heap (DMA-capable DRAM)
    uint32_t buf_size = screenWidth * screenHeight / 10;  // 7680 — matches test-lvgl
    buf = (lv_color_t*)malloc(buf_size * sizeof(lv_color_t));
    if (!buf) {
        Serial.println("[UGENT] FAIL: LVGL draw buffer malloc");
        return false;
    }
    Serial.printf("[UGENT] Draw buffer: %u pixels (%u bytes) on heap at %p\n",
                  buf_size, buf_size * sizeof(lv_color_t), buf);

    lv_disp_draw_buf_init(&draw_buf, buf, NULL, buf_size);

    // ─── Step 5: Register LVGL display driver ─────────────────────────────
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = screenWidth;
    disp_drv.ver_res = screenHeight;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    // ─── Step 6: Register LVGL input driver ───────────────────────────────
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touchpad_read;
    lv_indev_drv_register(&indev_drv);

    Serial.println("[UGENT] LVGL drivers registered");

    // ─── Step 7: Backlight PWM ────────────────────────────────────────────
    ugent_ledc_init(LCD_BL_PIN, BACKLIGHT_PWM_CHANNEL,
                    BACKLIGHT_PWM_FREQ, BACKLIGHT_PWM_RES);
    // Don't read NVS here — it hasn't been initialized yet
    ugent_ledc_write(LCD_BL_PIN, BACKLIGHT_PWM_CHANNEL, 128);

    // ─── Step 8: DIAGNOSTIC — render colored test (like test-lvgl) ───────
    // This proves LVGL works before we add WiFi complexity
    {
        lv_obj_t *scr = lv_scr_act();
        lv_obj_set_style_bg_color(scr, lv_color_hex(0x1E1E2E), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);

        lv_obj_t *btn1 = lv_btn_create(scr);
        lv_obj_set_size(btn1, 140, 90);
        lv_obj_align(btn1, LV_ALIGN_TOP_LEFT, 10, 10);
        lv_obj_set_style_bg_color(btn1, lv_color_hex(0xFF0000), LV_PART_MAIN);
        lv_obj_t *l1 = lv_label_create(btn1);
        lv_label_set_text(l1, "TEST");
        lv_obj_center(l1);

        lv_obj_t *btn2 = lv_btn_create(scr);
        lv_obj_set_size(btn2, 140, 90);
        lv_obj_align(btn2, LV_ALIGN_TOP_RIGHT, -10, 10);
        lv_obj_set_style_bg_color(btn2, lv_color_hex(0x00FF00), LV_PART_MAIN);
        lv_obj_t *l2 = lv_label_create(btn2);
        lv_label_set_text(l2, "COLOR");
        lv_obj_set_style_text_color(l2, lv_color_hex(0x000000), LV_PART_MAIN);
        lv_obj_center(l2);

        lv_obj_t *btn3 = lv_btn_create(scr);
        lv_obj_set_size(btn3, 140, 90);
        lv_obj_align(btn3, LV_ALIGN_BOTTOM_LEFT, 10, -10);
        lv_obj_set_style_bg_color(btn3, lv_color_hex(0x0000FF), LV_PART_MAIN);

        lv_obj_t *btn4 = lv_btn_create(scr);
        lv_obj_set_size(btn4, 140, 90);
        lv_obj_align(btn4, LV_ALIGN_BOTTOM_RIGHT, -10, -10);
        lv_obj_set_style_bg_color(btn4, lv_color_hex(0xFFFF00), LV_PART_MAIN);

        Serial.println("[UGENT] Diagnostic: 4 colored buttons rendered");
        Serial.println("[UGENT] If you see RED/GREEN/BLUE/YELLOW -> LVGL works!");
        Serial.println("[UGENT] If B&W -> display driver issue, not WiFi");
    }

    // Render the diagnostic for 3 seconds before proceeding
    Serial.println("[UGENT] Showing diagnostic for 3 seconds...");
    for (int i = 0; i < 600; i++) {  // 600 * 5ms = 3 seconds
        lv_timer_handler();
        delay(5);
    }

    // Check LVGL memory usage
    lv_mem_monitor_t mon;
    lv_mem_monitor(&mon);
    Serial.printf("[UGENT] LVGL mem: total=%u, free=%u, used=%u (%u%%), frag=%u%%\n",
                  mon.total_size, mon.free_size, mon.total_size - mon.free_size,
                  mon.used_pct, mon.frag_pct);

    // Clear diagnostic — delete all children of screen
    lv_obj_clean(lv_scr_act());

    Serial.println("[UGENT] System init OK");
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
    if (!init_system()) {
        Serial.println("[UGENT] System init FAILED");
        // Can't use LVGL if init failed — raw TFT error screen
        tft.fillScreen(TFT_RED);
        tft.setTextColor(TFT_WHITE);
        tft.drawString("INIT ERROR", 80, 100, 2);
        while (true) delay(1000);
    }

    // WiFi/NVS/SSE AFTER LVGL is confirmed working
    init_connectivity();

    // Create the real UI
    ui.setClients(&ugent, &wifi, &nvs);
    ui.begin();
    interact_history_init();

    // Final LVGL memory check
    lv_mem_monitor_t mon;
    lv_mem_monitor(&mon);
    Serial.printf("[UGENT] LVGL mem after UI: total=%u, free=%u, used=%u (%u%%)\n",
                  mon.total_size, mon.free_size,
                  mon.total_size - mon.free_size, mon.used_pct);
    if (mon.used_pct > 90) {
        Serial.println("[UGENT] WARNING: LVGL memory >90% — may cause rendering issues!");
    }

    if (wifi.isConnected() && ugent.testConnection()) {
        ugent.fetchStatus();
        ui.updateDashboard(&ugent);
        ui.updateTasks(&ugent);
    }

    // Update backlight from NVS now that NVS is initialized
    currentBrightness = nvs.getBrightness();
    if (currentBrightness < 10) currentBrightness = 128;
    ugent_ledc_write(LCD_BL_PIN, BACKLIGHT_PWM_CHANNEL, currentBrightness);

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
