/**
 * UGENT ESP32 Monitor — UI Manager
 *
 * Manages LVGL tab-based UI with 4 screens:
 *   Dashboard, Tasks, Interaction, Settings
 *
 * NOTE: LVGL display/touch driver init is done in the main .ino file.
 *       This class ONLY creates LVGL widgets.
 */

#ifndef UI_MANAGER_H
#define UI_MANAGER_H

#include <lvgl.h>
#include "ui_theme.h"
#include "../config.h"
#include "../nvs_storage.h"
#include "../wifi_manager.h"
#include "../ugent_client.h"
#include "screen_dashboard.h"
#include "screen_tasks.h"
#include "screen_interact.h"
#include "screen_settings.h"

enum Screen : uint8_t {
    SCREEN_DASHBOARD = 0,
    SCREEN_TASKS,
    SCREEN_INTERACT,
    SCREEN_SETTINGS,
    SCREEN_COUNT
};

class UIManager {
public:
    void begin() {
        // Apply theme
        theme_init();

        // Style the active screen background
        lv_obj_t* scr = lv_scr_act();
        lv_obj_set_style_bg_color(scr, color_bg(), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);

        // Create tabview with bottom tab bar (36px height)
        tabview_ = lv_tabview_create(scr, LV_DIR_BOTTOM, 36);
        lv_obj_set_size(tabview_, SCREEN_WIDTH, SCREEN_HEIGHT);
        lv_obj_set_style_bg_color(tabview_, color_bg(), LV_PART_MAIN);

        // Style tab buttons
        lv_obj_t* tab_btns = lv_tabview_get_tab_btns(tabview_);
        lv_obj_set_style_bg_color(tab_btns, color_surface0(), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(tab_btns, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_text_color(tab_btns, color_text(), LV_PART_ITEMS);
        lv_obj_set_style_text_font(tab_btns, &lv_font_montserrat_10, LV_PART_ITEMS);
        lv_obj_set_style_bg_color(tab_btns, color_surface1(),
                                  LV_PART_ITEMS | LV_STATE_CHECKED);
        lv_obj_set_style_text_color(tab_btns, color_blue(),
                                  LV_PART_ITEMS | LV_STATE_CHECKED);
        lv_obj_set_style_border_side(tab_btns, LV_BORDER_SIDE_BOTTOM,
                                     LV_PART_ITEMS | LV_STATE_CHECKED);
        lv_obj_set_style_border_color(tab_btns, color_blue(),
                                      LV_PART_ITEMS | LV_STATE_CHECKED);
        lv_obj_set_style_border_width(tab_btns, 2,
                                      LV_PART_ITEMS | LV_STATE_CHECKED);

        // Create tabs
        lv_obj_t* tab_dash = lv_tabview_add_tab(tabview_, "Home");
        lv_obj_t* tab_task = lv_tabview_add_tab(tabview_, "Tasks");
        lv_obj_t* tab_int  = lv_tabview_add_tab(tabview_, "Chat");
        lv_obj_t* tab_set  = lv_tabview_add_tab(tabview_, "Setup");

        // Set bg for each tab
        lv_obj_t* tabs[] = {tab_dash, tab_task, tab_int, tab_set};
        for (uint8_t i = 0; i < SCREEN_COUNT; i++) {
            lv_obj_set_style_bg_color(tabs[i], color_bg(), LV_PART_MAIN);
            lv_obj_set_style_bg_opa(tabs[i], LV_OPA_COVER, LV_PART_MAIN);
            lv_obj_clear_flag(tabs[i], LV_OBJ_FLAG_SCROLLABLE);
        }

        // Build each screen's widgets
        screen_dashboard_create(tab_dash);
        screen_tasks_create(tab_task);
        screen_interact_create(tab_int);
        // Settings screen needs NVS, WiFi, UGENT client for callbacks
        if (nvs_ && wifi_ && ugent_) {
            screen_settings_create(tab_set, nvs_, wifi_, ugent_);
        }

        // Wire up interaction client
        if (ugent_) {
            screen_interact_set_client(ugent_);
        }
    }

    void setClients(UgentClient* ugent, WifiManager* wifi, NvsStorage* nvs) {
        ugent_ = ugent;
        wifi_  = wifi;
        nvs_   = nvs;
    }

    // ─── Screen Updates ───────────────────────────────────────────────────

    void updateDashboard(UgentClient* client) {
        screen_dashboard_update(client);
    }

    void updateTasks(UgentClient* client) {
        screen_tasks_update(client);
    }

    void showInteraction(const char* question) {
        screen_interact_show_question(question);
    }

    void showAlert(const char* message) {
        // TODO: show a modal/popup alert
    }

    Screen getCurrentScreen() const {
        return currentScreen_;
    }

    void setScreen(Screen screen) {
        currentScreen_ = screen;
        lv_tabview_set_act(tabview_, screen, LV_ANIM_ON);
    }

private:
    lv_obj_t* tabview_ = nullptr;
    Screen currentScreen_ = SCREEN_DASHBOARD;

    // Pointers owned by main .ino — not freed here
    UgentClient* ugent_ = nullptr;
    WifiManager* wifi_  = nullptr;
    NvsStorage*  nvs_   = nullptr;
};

#endif // UI_MANAGER_H
