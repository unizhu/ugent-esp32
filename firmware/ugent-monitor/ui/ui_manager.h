/**
 * UGENT ESP32 Monitor — UI Manager
 *
 * LVGL initialization, screen management, and tab navigation.
 */

#ifndef UI_MANAGER_H
#define UI_MANAGER_H

#include <lvgl.h>
#include <TFT_eSPI.h>
#include <TFT_Touch.h>
#include "config.h"
#include "ui_theme.h"

// Forward declarations for screen create functions
void screen_dashboard_create(lv_obj_t* parent);
void screen_dashboard_update(class UgentClient* client);
void screen_tasks_create(lv_obj_t* parent);
void screen_tasks_update(class UgentClient* client);
void screen_interact_create(lv_obj_t* parent);
void screen_settings_create(lv_obj_t* parent, class NvsStorage* nvs,
                            class WifiManager* wifi, class UgentClient* ugent);

class UiManager {
public:
    enum Screen : uint8_t {
        SCREEN_DASHBOARD = 0,
        SCREEN_TASKS     = 1,
        SCREEN_INTERACT  = 2,
        SCREEN_SETTINGS  = 3,
        SCREEN_COUNT     = 4
    };

    void begin() {
        // Initialize TFT
        tft_.begin();
        tft_.setRotation(TFT_ROTATION);
        tft_.fillScreen(TFT_BLACK);

        // Initialize touch
        touch_.begin();
        touch_.setCal(TOUCH_CAL_0, TOUCH_CAL_1, TOUCH_CAL_2, TOUCH_CAL_3,
                      SCREEN_WIDTH, SCREEN_HEIGHT, 1);

        // Initialize LVGL
        lv_init();

        // Draw buffer — partial buffer to save RAM
        lv_disp_draw_buf_init(&draw_buf_, buf_, NULL, LVGL_BUF_SIZE);

        // Display driver
        lv_disp_drv_init(&disp_drv_);
        disp_drv_.hor_res = SCREEN_WIDTH;
        disp_drv_.ver_res = SCREEN_HEIGHT;
        disp_drv_.flush_cb = dispFlush;
        disp_drv_.draw_buf = &draw_buf_;
        lv_disp_drv_register(&disp_drv_);

        // Input (touch) driver
        lv_indev_drv_init(&indev_drv_);
        indev_drv_.type = LV_INDEV_TYPE_POINTER;
        indev_drv_.read_cb = touchpadRead;
        lv_indev_drv_register(&indev_drv_);

        // Apply theme
        theme_init();

        // Create main screen with tab navigation
        createMainScreen();
    }

    void tick() {
        lv_timer_handler();
    }

    void setScreen(Screen screen) {
        currentScreen_ = screen;
        lv_tabview_set_act(tabview_, screen, LV_ANIM_ON);

        // Update tab button styles
        for (uint8_t i = 0; i < SCREEN_COUNT; i++) {
            lv_obj_t* btn = lv_tabview_get_tab_btns(tabview_);
            // Apply active/inactive style based on selection
        }
    }

    Screen getCurrentScreen() const { return currentScreen_; }

    // Access to sub-screens for updates
    lv_obj_t* getScreen(Screen s) const { return screens_[s]; }

    // Set brightness (0-100)
    void setBrightness(uint8_t percent) {
        // Map 0-100 to PWM range (0-255)
        uint8_t pwm = (uint8_t)((percent * 255) / 100);
        ledcWrite(0, pwm);
    }

private:
    void createMainScreen() {
        // Main screen with bg color
        lv_obj_t* scr = lv_scr_act();
        lv_obj_add_style(scr, &style_scr, 0);

        // Tabview with bottom tab bar
        tabview_ = lv_tabview_create(scr, LV_DIR_BOTTOM, 36);
        lv_obj_set_size(tabview_, SCREEN_WIDTH, SCREEN_HEIGHT);
        lv_obj_set_style_bg_color(tabview_, color_bg(), LV_PART_MAIN);

        // Style the tab buttons area
        lv_obj_t* tab_btns = lv_tabview_get_tab_btns(tabview_);
        lv_obj_set_style_bg_color(tab_btns, color_surface0(), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(tab_btns, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_text_color(tab_btns, color_text(), LV_PART_ITEMS);
        lv_obj_set_style_text_font(tab_btns, &lv_font_montserrat_10, LV_PART_ITEMS);
        lv_obj_set_style_bg_color(tab_btns, color_surface1(), LV_PART_ITEMS | LV_STATE_CHECKED);
        lv_obj_set_style_text_color(tab_btns, color_blue(), LV_PART_ITEMS | LV_STATE_CHECKED);
        lv_obj_set_style_border_side(tab_btns, LV_BORDER_SIDE_BOTTOM, LV_PART_ITEMS | LV_STATE_CHECKED);
        lv_obj_set_style_border_color(tab_btns, color_blue(), LV_PART_ITEMS | LV_STATE_CHECKED);
        lv_obj_set_style_border_width(tab_btns, 2, LV_PART_ITEMS | LV_STATE_CHECKED);

        // Create tabs (screens)
        screens_[SCREEN_DASHBOARD] = lv_tabview_add_tab(tabview_, "Home");
        screens_[SCREEN_TASKS]     = lv_tabview_add_tab(tabview_, "Tasks");
        screens_[SCREEN_INTERACT]  = lv_tabview_add_tab(tabview_, "Chat");
        screens_[SCREEN_SETTINGS]  = lv_tabview_add_tab(tabview_, "Setup");

        // Set bg for each tab content area
        for (uint8_t i = 0; i < SCREEN_COUNT; i++) {
            lv_obj_set_style_bg_color(screens_[i], color_bg(), LV_PART_MAIN);
            lv_obj_set_style_bg_opa(screens_[i], LV_OPA_COVER, LV_PART_MAIN);
            lv_obj_clear_flag(screens_[i], LV_OBJ_FLAG_SCROLLABLE);
        }
    }

    // ─── LVGL Callbacks ───────────────────────────────────────────────────

    static void dispFlush(lv_disp_drv_t* disp, const lv_area_t* area,
                          lv_color_t* color_p) {
        uint32_t w = area->x2 - area->x1 + 1;
        uint32_t h = area->y2 - area->y1 + 1;

        tft_.startWrite();
        tft_.setAddrWindow(area->x1, area->y1, w, h);
        tft_.pushColors((uint16_t*)&color_p->full, w * h, true);
        tft_.endWrite();

        lv_disp_flush_ready(disp);
    }

    static void touchpadRead(lv_indev_drv_t* indev_driver,
                             lv_indev_data_t* data) {
        bool touched = touch_.Pressed();

        if (!touched) {
            data->state = LV_INDEV_STATE_REL;
        } else {
            data->state = LV_INDEV_STATE_PR;
            data->point.x = touch_.X();
            data->point.y = touch_.Y();
        }
    }

    // ─── Members ──────────────────────────────────────────────────────────

    static TFT_eSPI tft_;
    static TFT_Touch touch_;
    static lv_disp_draw_buf_t draw_buf_;
    static lv_color_t buf_[];
    static lv_disp_drv_t disp_drv_;
    static lv_indev_drv_t indev_drv_;

    lv_obj_t* tabview_;
    lv_obj_t* screens_[SCREEN_COUNT];
    Screen currentScreen_ = SCREEN_DASHBOARD;
};

// Static member definitions
static TFT_eSPI UiManager_tft(SCREEN_WIDTH, SCREEN_HEIGHT);
static TFT_Touch UiManager_touch(TOUCH_CS, TOUCH_CLK, TOUCH_DIN, TOUCH_DOUT);
static lv_color_t UiManager_buf_[LVGL_BUF_SIZE];
static lv_disp_draw_buf_t UiManager_draw_buf_;
static lv_disp_drv_t UiManager_disp_drv_;
static lv_indev_drv_t UiManager_indev_drv_;

// Assign to class static members
TFT_eSPI      UiManager::tft_ = UiManager_tft;
TFT_Touch     UiManager::touch_ = UiManager_touch;
lv_color_t    UiManager::buf_[] = {0};
lv_disp_draw_buf_t UiManager::draw_buf_ = {};
lv_disp_drv_t UiManager::disp_drv_ = {};
lv_indev_drv_t UiManager::indev_drv_ = {};

#endif // UI_MANAGER_H
