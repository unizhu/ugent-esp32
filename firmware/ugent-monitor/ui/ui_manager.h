/**
 * UGENT ESP32 Monitor — UI Manager
 *
 * Manages LVGL tab-based UI with 4 screens:
 *   Dashboard, Tasks, Interaction, Settings
 *
 * Settings screen is LAZY-LOADED: only created when user taps Setup tab.
 * This saves ~15 LVGL objects and reduces boot-time memory by ~3KB.
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

// Global so the static LVGL event callback can access it
static UIManager* g_ui_instance = nullptr;

class UIManager {
public:
    void begin() {
        g_ui_instance = this;

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
        tab_dash_ = lv_tabview_add_tab(tabview_, "Home");
        tab_task_ = lv_tabview_add_tab(tabview_, "Tasks");
        tab_int_  = lv_tabview_add_tab(tabview_, "Chat");
        tab_set_  = lv_tabview_add_tab(tabview_, "Setup");

        // Set bg for each tab
        lv_obj_t* tabs[] = {tab_dash_, tab_task_, tab_int_, tab_set_};
        for (uint8_t i = 0; i < SCREEN_COUNT; i++) {
            lv_obj_set_style_bg_color(tabs[i], color_bg(), LV_PART_MAIN);
            lv_obj_set_style_bg_opa(tabs[i], LV_OPA_COVER, LV_PART_MAIN);
            lv_obj_clear_flag(tabs[i], LV_OBJ_FLAG_SCROLLABLE);
        }

        // Build visible screens only (Dashboard, Tasks, Chat)
        screen_dashboard_create(tab_dash_);
        screen_tasks_create(tab_task_);
        screen_interact_create(tab_int_);

        // Settings tab: show placeholder, lazy-load on first visit
        lv_obj_t* placeholder = lv_label_create(tab_set_);
        lv_label_set_text(placeholder, "Tap here to load Setup");
        lv_obj_set_style_text_color(placeholder, color_overlay0(), LV_PART_MAIN);
        lv_obj_center(placeholder);

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

    // Called from main loop — handles lazy loading of settings tab
    void loop() {
        if (!settings_created_ && tabview_) {
            uint16_t active = lv_tabview_get_tab_act(tabview_);
            if (active == SCREEN_SETTINGS && nvs_ && wifi_ && ugent_) {
                // User tapped Setup tab — create the settings UI now
                lv_obj_clean(tab_set_);  // Remove placeholder
                lv_obj_clear_flag(tab_set_, LV_OBJ_FLAG_SCROLLABLE);
                screen_settings_create(tab_set_, nvs_, wifi_, ugent_);
                settings_created_ = true;
            }
        }
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
    lv_obj_t* tab_dash_ = nullptr;
    lv_obj_t* tab_task_ = nullptr;
    lv_obj_t* tab_int_  = nullptr;
    lv_obj_t* tab_set_  = nullptr;
    bool settings_created_ = false;
    Screen currentScreen_ = SCREEN_DASHBOARD;

    // Pointers owned by main .ino — not freed here
    UgentClient* ugent_ = nullptr;
    WifiManager* wifi_  = nullptr;
    NvsStorage*  nvs_   = nullptr;
};

#endif // UI_MANAGER_H
