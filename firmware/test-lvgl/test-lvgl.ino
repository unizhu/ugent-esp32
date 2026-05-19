/**
 * Minimal LVGL Test for ESP32-2432S028R (Cheap Yellow Display)
 * 
 * Tests LVGL rendering WITHOUT WiFi/NVS/SSE.
 * Uses witnessmenow configuration exactly:
 *   - Display on HSPI via TFT_eSPI with USE_HSPI_PORT
 *   - Touch on separate VSPI via XPT2046_Touchscreen
 *
 * Expected: Colored LVGL buttons and labels.
 * If B&W or flickering → LVGL/TFT_eSPI integration issue.
 * If correct colors → ugent-monitor's WiFi/NVS/SSE code is the problem.
 *
 * REQUIRED:
 *   - TFT_eSPI (Library Manager)
 *   - XPT2046_Touchscreen by Paul Stoffregen
 *   - LVGL 8.x (install + move examples/demos into src/)
 *   - Copy firmware/User_Setup.h to TFT_eSPI library
 *   - Copy firmware/lv_conf.h NEXT TO the lvgl library folder
 */

#include <lvgl.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <SPI.h>

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
// Using witnessmenow's exact pattern: landscape dims + static buffer
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[screenWidth * screenHeight / 10];  // ~7680 bytes static

TFT_eSPI tft = TFT_eSPI(screenWidth, screenHeight);

SPIClass touchSpi = SPIClass(VSPI);
XPT2046_Touchscreen ts(XPT2046_CS, XPT2046_IRQ);

uint16_t touchMinX = 200, touchMaxX = 3700;
uint16_t touchMinY = 240,  touchMaxY = 3800;

// ─── Display Flush (exact copy from witnessmenow LVGL8 example) ────────────────
void my_disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors((uint16_t *)&color_p->full, w * h, true);
    tft.endWrite();

    lv_disp_flush_ready(disp_drv);
}

// ─── Touch Read (from witnessmenow LVGL8 example) ─────────────────────────────
void my_touchpad_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data) {
    if (ts.touched()) {
        TS_Point p = ts.getPoint();
        if (p.x < touchMinX) touchMinX = p.x;
        if (p.x > touchMaxX) touchMaxX = p.x;
        if (p.y < touchMinY) touchMinY = p.y;
        if (p.y > touchMaxY) touchMaxY = p.y;
        data->point.x = map(p.x, touchMinX, touchMaxX, 1, screenWidth);
        data->point.y = map(p.y, touchMinY, touchMaxY, 1, screenHeight);
        data->state = LV_INDEV_STATE_PR;
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("=== LVGL Test (witnessmenow config) ===");

    lv_init();

    // Touch init FIRST (separate VSPI bus)
    touchSpi.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
    ts.begin(touchSpi);
    ts.setRotation(1);

    // TFT init
    tft.begin();
    tft.setRotation(1);

    // LVGL display buffer — witnessmenow pattern
    lv_disp_draw_buf_init(&draw_buf, buf, NULL, screenWidth * screenHeight / 10);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = screenWidth;
    disp_drv.ver_res = screenHeight;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    // LVGL input device
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touchpad_read;
    lv_indev_drv_register(&indev_drv);

    // ─── Create colored test UI ────────────────────────────────────────────────
    
    // Background
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x1E1E2E), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);

    // Red button (top-left)
    lv_obj_t *btn1 = lv_btn_create(scr);
    lv_obj_set_size(btn1, 140, 90);
    lv_obj_align(btn1, LV_ALIGN_TOP_LEFT, 10, 10);
    lv_obj_set_style_bg_color(btn1, lv_color_hex(0xFF0000), LV_PART_MAIN);
    lv_obj_t *label1 = lv_label_create(btn1);
    lv_label_set_text(label1, "RED");
    lv_obj_center(label1);

    // Green button (top-right)
    lv_obj_t *btn2 = lv_btn_create(scr);
    lv_obj_set_size(btn2, 140, 90);
    lv_obj_align(btn2, LV_ALIGN_TOP_RIGHT, -10, 10);
    lv_obj_set_style_bg_color(btn2, lv_color_hex(0x00FF00), LV_PART_MAIN);
    lv_obj_t *label2 = lv_label_create(btn2);
    lv_label_set_text(label2, "GREEN");
    lv_obj_set_style_text_color(label2, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_center(label2);

    // Blue button (bottom-left)
    lv_obj_t *btn3 = lv_btn_create(scr);
    lv_obj_set_size(btn3, 140, 90);
    lv_obj_align(btn3, LV_ALIGN_BOTTOM_LEFT, 10, -10);
    lv_obj_set_style_bg_color(btn3, lv_color_hex(0x0000FF), LV_PART_MAIN);
    lv_obj_t *label3 = lv_label_create(btn3);
    lv_label_set_text(label3, "BLUE");
    lv_obj_center(label3);

    // Yellow button (bottom-right)
    lv_obj_t *btn4 = lv_btn_create(scr);
    lv_obj_set_size(btn4, 140, 90);
    lv_obj_align(btn4, LV_ALIGN_BOTTOM_RIGHT, -10, -10);
    lv_obj_set_style_bg_color(btn4, lv_color_hex(0xFFFF00), LV_PART_MAIN);
    lv_obj_t *label4 = lv_label_create(btn4);
    lv_label_set_text(label4, "YELLOW");
    lv_obj_set_style_text_color(label4, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_center(label4);

    Serial.println("LVGL UI created. If colors are correct, LVGL integration works.");
    Serial.println("If B&W or flickering, check lv_conf.h and User_Setup.h.");
}

void loop() {
    lv_timer_handler();
    delay(5);
}
