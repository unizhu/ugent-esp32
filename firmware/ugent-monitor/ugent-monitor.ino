/**
 * UGENT ESP32 Monitor — Main Entry Point
 *
 * Firmware for ESP32-2432S028R (2.8" TFT + Touch)
 * Displays UGENT agent status, tasks, and enables human-in-the-loop interaction.
 *
 * HARDWARE NOTES:
 *   Display (ILI9341) and Touch (XPT2046) are on SEPARATE SPI buses.
 *   - Display SPI (HSPI): MOSI=13, SCLK=14, CS=15, DC=2, RST=12
 *   - Touch SPI (VSPI):   MISO=39, MOSI=32, CS=33, CLK=25
 *
 * COLOR NOTES:
 *   ILI9341 over SPI needs byte-swapping for RGB565.
 *   LV_COLOR_16_SWAP must be 0 (disabled) in lv_conf.h.
 *   The display flush uses pushColors(..., true) to let TFT_eSPI handle the swap.
 *   This matches the vendor example AND the official LVGL TFT_eSPI driver.
 *
 * TOUCH NOTES:
 *   We use raw SPI (hardware SPIClass) for touch instead of the TFT_Touch library.
 *   TFT_Touch uses bit-banged GPIO with multiple delay(1) calls that block for
 *   ~20ms per read, causing display flickering. Hardware SPI is ~10x faster.
 */

#include <Arduino.h>
#include <lvgl.h>
#include <TFT_eSPI.h>
#include <SPI.h>
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

// LVGL display driver buffers
static lv_disp_draw_buf_t draw_buf;
static lv_color_t* buf1 = nullptr;
static lv_color_t* buf2 = nullptr;  // Double buffer for flicker-free rendering
static lv_indev_drv_t indev_drv;

// Touch SPI — separate hardware SPI bus (VSPI) for XPT2046
static SPIClass touchSPI(VSPI);

// Last touch position (for LVGL continuity when released)
static int16_t last_touch_x = 0;
static int16_t last_touch_y = 0;

// Backlight PWM
static uint8_t currentBrightness = 128;

// ─── Display Flush ─────────────────────────────────────────────────────────────
// Matches vendor example and official LVGL TFT_eSPI driver:
// pushColors with byteSwap=true, LV_COLOR_16_SWAP=0

static void disp_flush(lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* color_p) {
    uint32_t w = area->x2 - area->x1 + 1;
    uint32_t h = area->y2 - area->y1 + 1;
    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors((uint16_t*)color_p, w * h, true);
    tft.endWrite();
    lv_disp_flush_ready(drv);
}

// ─── XPT2046 Touch via Hardware SPI ────────────────────────────────────────────
// Direct SPI register reads — ~100x faster than bit-banged TFT_Touch library.
// Based on ropg/LVGL_CYD implementation with pressure-based touch detection.

#define XPT2046_Z1_THRESHOLD 300

// Out of three values, returns average of the two closest together
static int16_t besttwoavg(int16_t a, int16_t b, int16_t c) {
    int16_t da = abs(a - b);
    int16_t db = abs(b - c);
    int16_t dc = abs(c - a);
    if (da <= db && da <= dc) return (a + b) / 2;
    if (db <= da && db <= dc) return (b + c) / 2;
    return (a + c) / 2;
}

static void touch_read(lv_indev_drv_t* drv, lv_indev_data_t* data) {    // Read pressure via Z1/Z2 measurement
    touchSPI.beginTransaction(SPISettings(2000000, MSBFIRST, SPI_MODE0));
    digitalWrite(TOUCH_CS, LOW);
    touchSPI.transfer(0xB1);  // Z1 command
    int16_t z1 = touchSPI.transfer16(0xC1) >> 3;  // Read Z1, start Z2
    int16_t z2 = touchSPI.transfer16(0x91) >> 3;   // Read Z2, start X dummy
    int16_t pressure = z1 + 4095 - z2;

    if (pressure < XPT2046_Z1_THRESHOLD) {
        digitalWrite(TOUCH_CS, HIGH);
        touchSPI.endTransaction();
        data->state = LV_INDEV_STATE_REL;
        data->point.x = last_touch_x;
        data->point.y = last_touch_y;
        return;
    }

    // Touch detected — read X,Y positions (3 samples for averaging)
    touchSPI.transfer16(0x91);  // Dummy X read (first read is noisy)
    int16_t x0 = touchSPI.transfer16(0xD1) >> 3;  // Y1
    int16_t y0 = touchSPI.transfer16(0x91) >> 3;  // X1
    int16_t x1 = touchSPI.transfer16(0xD1) >> 3;  // Y2
    int16_t y1 = touchSPI.transfer16(0x91) >> 3;  // X2
    int16_t x2 = touchSPI.transfer16(0xD0) >> 3;  // Y3 (power down)
    int16_t y2 = touchSPI.transfer16(0x00) >> 3;  // X3 (power down)
    digitalWrite(TOUCH_CS, HIGH);
    touchSPI.endTransaction();

    // Average of closest two readings (filter outliers)
    int16_t rawY = besttwoavg(x0, x1, x2);
    int16_t rawX = besttwoavg(y0, y1, y2);

    // Map raw ADC to screen coordinates
    // Vendor calibration: setCal(526, 3443, 750, 3377, 320, 240, 1)
    // With axis swap=1, rawX maps to screen Y, rawY maps to screen X
    int16_t x = map(rawY, TOUCH_CAL_XMIN, TOUCH_CAL_XMAX, 0, SCREEN_WIDTH - 1);
    int16_t y = map(rawX, TOUCH_CAL_YMIN, TOUCH_CAL_YMAX, SCREEN_HEIGHT - 1, 0);

    x = constrain(x, 0, SCREEN_WIDTH - 1);
    y = constrain(y, 0, SCREEN_HEIGHT - 1);

    last_touch_x = x;
    last_touch_y = y;

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

    // Touch SPI init — XPT2046 on VSPI bus (separate from display HSPI)
    touchSPI.begin(TOUCH_CLK, TOUCH_DOUT, TOUCH_DIN, TOUCH_CS);
    pinMode(TOUCH_CS, OUTPUT);
    digitalWrite(TOUCH_CS, HIGH);

    Serial.println("[UGENT] Touch SPI initialized (hardware SPI)");

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
    delay(2);

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
