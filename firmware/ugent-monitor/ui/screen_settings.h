/**
 * UGENT ESP32 Monitor — Settings Screen
 *
 * WiFi scan & select, UGENT server config, brightness control.
 * Supports up to 3 saved WiFi networks.
 */

#ifndef SCREEN_SETTINGS_H
#define SCREEN_SETTINGS_H

#include <lvgl.h>
#include "ui_theme.h"
#include "../config.h"
#include "../nvs_storage.h"
#include "../wifi_manager.h"
#include "../ugent_client.h"

static NvsStorage* settings_nvs_ = nullptr;
static WifiManager* settings_wifi_ = nullptr;
static UgentClient* settings_ugent_ = nullptr;

static lv_obj_t* settings_wifi_status = nullptr;
static lv_obj_t* settings_wifi_ssid_ta = nullptr;
static lv_obj_t* settings_wifi_pass_ta = nullptr;
static lv_obj_t* settings_wifi_connect_btn = nullptr;
static lv_obj_t* settings_wifi_scan_btn = nullptr;
static lv_obj_t* settings_wifi_list_cont = nullptr;
static lv_obj_t* settings_saved_list_cont = nullptr;
static lv_obj_t* settings_ugent_host_ta = nullptr;
static lv_obj_t* settings_ugent_port_ta = nullptr;
static lv_obj_t* settings_ugent_apikey_ta = nullptr;
static lv_obj_t* settings_test_btn = nullptr;
static lv_obj_t* settings_brightness_slider = nullptr;
static lv_obj_t* settings_feedback = nullptr;
static lv_obj_t* settings_kb = nullptr;

// Scan result storage (heap-allocated on scan to minimize BSS)
static const int MAX_SCAN_DISPLAY = 8;
static String* scanned_ssids = nullptr;
static int scan_result_count = 0;

// ─── Keyboard handling ─────────────────────────────────────────────────────────

static lv_obj_t* settings_active_ta = nullptr;

static void ta_focus_cb(lv_event_t* e) {
    settings_active_ta = (lv_obj_t*)lv_event_get_target(e);
    if (settings_kb) {
        lv_keyboard_set_textarea(settings_kb, settings_active_ta);
    }
}

static void show_kb() {
    if (settings_kb) lv_obj_clear_flag(settings_kb, LV_OBJ_FLAG_HIDDEN);
}

static void hide_kb() {
    if (settings_kb) lv_obj_add_flag(settings_kb, LV_OBJ_FLAG_HIDDEN);
}

// ─── Forward declarations ──────────────────────────────────────────────────────
static void refresh_saved_list();

// ─── WiFi: Connect ─────────────────────────────────────────────────────────────

static void wifi_connect_cb(lv_event_t* e) {
    if (!settings_wifi_ || !settings_nvs_) return;

    const char* ssid = lv_textarea_get_text(settings_wifi_ssid_ta);
    const char* pass = lv_textarea_get_text(settings_wifi_pass_ta);

    if (strlen(ssid) == 0) {
        lv_label_set_text(settings_feedback, "Enter SSID");
        lv_obj_set_style_text_color(settings_feedback, color_yellow(), LV_PART_MAIN);
        return;
    }

    hide_kb();
    lv_label_set_text(settings_feedback, "Connecting...");
    lv_obj_set_style_text_color(settings_feedback, color_blue(), LV_PART_MAIN);
    lv_refr_now(NULL);

    if (settings_wifi_->connect(ssid, pass)) {
        lv_label_set_text(settings_feedback, "Connected!");
        lv_obj_set_style_text_color(settings_feedback, color_green(), LV_PART_MAIN);
        lv_label_set_text_fmt(settings_wifi_status,
            "%s (%s)", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
        refresh_saved_list();
    } else {
        lv_label_set_text(settings_feedback, "Failed!");
        lv_obj_set_style_text_color(settings_feedback, color_red(), LV_PART_MAIN);
    }
}

// ─── WiFi: Scan ────────────────────────────────────────────────────────────────

static void wifi_select_cb(lv_event_t* e) {
    int idx = (int)(intptr_t)lv_obj_get_user_data(lv_event_get_target(e));
    if (idx < 0 || idx >= scan_result_count) return;

    lv_textarea_set_text(settings_wifi_ssid_ta, scanned_ssids[idx].c_str());
    lv_textarea_set_text(settings_wifi_pass_ta, "");

    // Hide scan results
    if (settings_wifi_list_cont) {
        lv_obj_add_flag(settings_wifi_list_cont, LV_OBJ_FLAG_HIDDEN);
    }

    // Show keyboard focused on password field
    lv_obj_add_state(settings_wifi_pass_ta, LV_STATE_FOCUSED);
    if (settings_kb) {
        lv_keyboard_set_textarea(settings_kb, settings_wifi_pass_ta);
    }
    show_kb();

    lv_label_set_text_fmt(settings_feedback, "Selected: %s", scanned_ssids[idx].c_str());
    lv_obj_set_style_text_color(settings_feedback, color_blue(), LV_PART_MAIN);
}

static void wifi_scan_cb(lv_event_t* e) {
    if (!settings_wifi_) return;

    hide_kb();

    lv_label_set_text(settings_feedback, "Scanning...");
    lv_obj_set_style_text_color(settings_feedback, color_blue(), LV_PART_MAIN);
    lv_refr_now(NULL);

    int n = settings_wifi_->scanNetworks();
    scan_result_count = (n > MAX_SCAN_DISPLAY) ? MAX_SCAN_DISPLAY : n;

    // Allocate scan results on heap (freed on next scan)
    if (!scanned_ssids) {
        scanned_ssids = new String[MAX_SCAN_DISPLAY];
    }

    // Store SSIDs before clearing scan
    for (int i = 0; i < scan_result_count; i++) {
        scanned_ssids[i] = settings_wifi_->getScannedSsid(i);
    }

    // Clear old results
    if (settings_wifi_list_cont) {
        lv_obj_clean(settings_wifi_list_cont);
        lv_obj_clear_flag(settings_wifi_list_cont, LV_OBJ_FLAG_HIDDEN);
    }

    // Build result buttons
    for (int i = 0; i < scan_result_count; i++) {
        int32_t rssi = settings_wifi_->getScannedRssi(i);
        bool open = (settings_wifi_->getScannedAuth(i) == WIFI_AUTH_OPEN);

        lv_obj_t* btn = lv_btn_create(settings_wifi_list_cont);
        lv_obj_set_size(btn, SCREEN_WIDTH - 24, 24);
        lv_obj_set_style_bg_color(btn, color_surface1(), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_radius(btn, 3, LV_PART_MAIN);
        lv_obj_set_style_pad_all(btn, 2, LV_PART_MAIN);
        lv_obj_set_user_data(btn, (void*)(intptr_t)i);
        lv_obj_add_event_cb(btn, wifi_select_cb, LV_EVENT_CLICKED, nullptr);

        // Signal strength bars
        char bars[5] = "    ";
        int bar_count = 0;
        if (rssi > -50) bar_count = 4;
        else if (rssi > -60) bar_count = 3;
        else if (rssi > -70) bar_count = 2;
        else bar_count = 1;
        for (int b = 0; b < bar_count; b++) bars[b] = '|';

        char label[80];
        snprintf(label, sizeof(label), "%s %s %s",
                 scanned_ssids[i].c_str(), bars,
                 open ? "[OPEN]" : "");

        lv_obj_t* lbl = lv_label_create(btn);
        lv_label_set_text(lbl, label);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_10, LV_PART_MAIN);
        lv_obj_set_style_text_color(lbl, color_text(), LV_PART_MAIN);
        lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 2, 0);
    }

    settings_wifi_->clearScanResults();

    lv_label_set_text_fmt(settings_feedback, "Found %d networks — tap to select", scan_result_count);
    lv_obj_set_style_text_color(settings_feedback, color_green(), LV_PART_MAIN);
}

// ─── WiFi: Delete saved ────────────────────────────────────────────────────────

static void wifi_delete_cb(lv_event_t* e) {
    int idx = (int)(intptr_t)lv_obj_get_user_data(lv_event_get_target(e));
    if (!settings_wifi_ || idx < 0) return;

    settings_wifi_->deleteSaved(idx);
    refresh_saved_list();
    lv_label_set_text(settings_feedback, "Deleted");
    lv_obj_set_style_text_color(settings_feedback, color_yellow(), LV_PART_MAIN);
}

// ─── WiFi: Refresh saved list ──────────────────────────────────────────────────

static void refresh_saved_list() {
    if (!settings_saved_list_cont || !settings_nvs_) return;
    lv_obj_clean(settings_saved_list_cont);

    int count = settings_nvs_->getWifiCount();
    for (int i = 0; i < count && i < MAX_WIFI_SAVED; i++) {
        String ssid = settings_nvs_->getWifiSsid(i);
        if (ssid.length() == 0) continue;

        // Row container
        lv_obj_t* row = lv_obj_create(settings_saved_list_cont);
        lv_obj_set_size(row, SCREEN_WIDTH - 20, 24);
        lv_obj_set_style_bg_color(row, color_surface1(), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_radius(row, 3, LV_PART_MAIN);
        lv_obj_set_style_pad_all(row, 2, LV_PART_MAIN);
        lv_obj_set_style_border_width(row, 0, LV_PART_MAIN);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

        // SSID label
        lv_obj_t* lbl = lv_label_create(row);
        lv_label_set_text(lbl, ssid.c_str());
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_10, LV_PART_MAIN);
        lv_obj_set_style_text_color(lbl, color_subtext(), LV_PART_MAIN);
        lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 4, 0);

        // Mark currently connected
        if (settings_wifi_ && settings_wifi_->isConnected() &&
            WiFi.SSID() == ssid) {
            lv_obj_t* conn_lbl = lv_label_create(row);
            lv_label_set_text(conn_lbl, "*");
            lv_obj_set_style_text_color(conn_lbl, color_green(), LV_PART_MAIN);
            lv_obj_align(conn_lbl, LV_ALIGN_LEFT_MID, 4 + strlen(ssid.c_str()) * 6 + 4, 0);
        }

        // Delete button
        lv_obj_t* del_btn = lv_btn_create(row);
        lv_obj_set_size(del_btn, 20, 18);
        lv_obj_align(del_btn, LV_ALIGN_RIGHT_MID, 0, 0);
        lv_obj_set_style_bg_color(del_btn, color_red(), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(del_btn, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_radius(del_btn, 2, LV_PART_MAIN);
        lv_obj_set_user_data(del_btn, (void*)(intptr_t)i);
        lv_obj_add_event_cb(del_btn, wifi_delete_cb, LV_EVENT_CLICKED, nullptr);
        lv_obj_t* x_lbl = lv_label_create(del_btn);
        lv_label_set_text(x_lbl, "X");
        lv_obj_set_style_text_font(x_lbl, &lv_font_montserrat_10, LV_PART_MAIN);
        lv_obj_center(x_lbl);
    }

    if (count == 0) {
        lv_obj_t* lbl = lv_label_create(settings_saved_list_cont);
        lv_label_set_text(lbl, "No saved networks");
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_10, LV_PART_MAIN);
        lv_obj_set_style_text_color(lbl, color_overlay0(), LV_PART_MAIN);
    }
}

// ─── UGENT: Test Connection ────────────────────────────────────────────────────

static void test_conn_cb(lv_event_t* e) {
    if (!settings_nvs_ || !settings_ugent_) return;

    settings_nvs_->setUgentHost(lv_textarea_get_text(settings_ugent_host_ta));
    const char* portStr = lv_textarea_get_text(settings_ugent_port_ta);
    settings_nvs_->setUgentPort((uint16_t)atoi(portStr));
    settings_nvs_->setUgentApiKey(lv_textarea_get_text(settings_ugent_apikey_ta));

    hide_kb();
    lv_label_set_text(settings_feedback, "Testing...");
    lv_obj_set_style_text_color(settings_feedback, color_blue(), LV_PART_MAIN);
    lv_refr_now(NULL);

    if (settings_ugent_->testConnection()) {
        lv_label_set_text(settings_feedback, "Connected to UGENT!");
        lv_obj_set_style_text_color(settings_feedback, color_green(), LV_PART_MAIN);
    } else {
        lv_label_set_text(settings_feedback, "Connection failed!");
        lv_obj_set_style_text_color(settings_feedback, color_red(), LV_PART_MAIN);
    }
}

// ─── Brightness ────────────────────────────────────────────────────────────────

static void brightness_cb(lv_event_t* e) {
    if (!settings_nvs_) return;
    int val = lv_slider_get_value(settings_brightness_slider);
    settings_nvs_->setBrightness((uint8_t)val);
}

// ─── Create ────────────────────────────────────────────────────────────────────

inline void screen_settings_create(lv_obj_t* parent, NvsStorage* nvs,
                                   WifiManager* wifi, UgentClient* ugent) {
    settings_nvs_ = nvs;
    settings_wifi_ = wifi;
    settings_ugent_ = ugent;

    // Main container — scrollable
    lv_obj_t* cont = lv_obj_create(parent);
    lv_obj_set_size(cont, SCREEN_WIDTH - 8, SCREEN_HEIGHT - 44);
    lv_obj_align(cont, LV_ALIGN_TOP_MID, 0, 2);
    lv_obj_set_style_bg_color(cont, color_bg(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(cont, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(cont, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(cont, 4, LV_PART_MAIN);
    lv_obj_add_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(cont, LV_SCROLLBAR_MODE_ACTIVE);

    // ─── Feedback Bar ─────────────────────────────────────────────────────
    settings_feedback = lv_label_create(cont);
    lv_label_set_text(settings_feedback, "");
    lv_obj_add_style(settings_feedback, &style_small, 0);
    lv_obj_align(settings_feedback, LV_ALIGN_TOP_RIGHT, 0, 0);

    // ─── WiFi Section ─────────────────────────────────────────────────────
    lv_obj_t* wifi_title = lv_label_create(cont);
    lv_label_set_text(wifi_title, "WiFi");
    lv_obj_add_style(wifi_title, &style_title, 0);
    lv_obj_set_style_text_color(wifi_title, color_blue(), LV_PART_MAIN);
    lv_obj_align(wifi_title, LV_ALIGN_TOP_LEFT, 4, 0);

    settings_wifi_status = lv_label_create(cont);
    if (wifi && wifi->isConnected()) {
        lv_label_set_text_fmt(settings_wifi_status, "%s (%s)",
            WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
    } else {
        lv_label_set_text(settings_wifi_status, "Not connected");
    }
    lv_obj_add_style(settings_wifi_status, &style_small, 0);
    lv_obj_align_to(settings_wifi_status, wifi_title, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 2);

    // [Scan WiFi] button
    settings_wifi_scan_btn = lv_btn_create(cont);
    lv_obj_set_size(settings_wifi_scan_btn, SCREEN_WIDTH - 20, 28);
    lv_obj_align_to(settings_wifi_scan_btn, settings_wifi_status, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 4);
    lv_obj_add_style(settings_wifi_scan_btn, &style_btn_primary, 0);
    lv_obj_add_event_cb(settings_wifi_scan_btn, wifi_scan_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* scan_lbl = lv_label_create(settings_wifi_scan_btn);
    lv_label_set_text(scan_lbl, "Scan WiFi Networks");
    lv_obj_center(scan_lbl);

    // Scan results container (hidden until scan)
    settings_wifi_list_cont = lv_obj_create(cont);
    lv_obj_set_size(settings_wifi_list_cont, SCREEN_WIDTH - 20, LV_SIZE_CONTENT);
    lv_obj_align_to(settings_wifi_list_cont, settings_wifi_scan_btn, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 2);
    lv_obj_set_style_bg_color(settings_wifi_list_cont, color_bg(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(settings_wifi_list_cont, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(settings_wifi_list_cont, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(settings_wifi_list_cont, color_surface2(), LV_PART_MAIN);
    lv_obj_set_style_pad_all(settings_wifi_list_cont, 2, LV_PART_MAIN);
    lv_obj_add_flag(settings_wifi_list_cont, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(settings_wifi_list_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(settings_wifi_list_cont, LV_SCROLLBAR_MODE_ACTIVE);
    lv_obj_set_style_max_height(settings_wifi_list_cont, 100, LV_PART_MAIN);

    // SSID input
    settings_wifi_ssid_ta = lv_textarea_create(cont);
    lv_obj_set_size(settings_wifi_ssid_ta, SCREEN_WIDTH - 20, 24);
    lv_obj_align_to(settings_wifi_ssid_ta, settings_wifi_list_cont, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 4);
    lv_textarea_set_placeholder_text(settings_wifi_ssid_ta, "WiFi SSID");
    lv_textarea_set_one_line(settings_wifi_ssid_ta, true);
    lv_obj_set_style_bg_color(settings_wifi_ssid_ta, color_surface0(), LV_PART_MAIN);
    lv_obj_set_style_text_color(settings_wifi_ssid_ta, color_text(), LV_PART_MAIN);
    lv_obj_add_event_cb(settings_wifi_ssid_ta, ta_focus_cb, LV_EVENT_FOCUSED, nullptr);

    // Password input
    settings_wifi_pass_ta = lv_textarea_create(cont);
    lv_obj_set_size(settings_wifi_pass_ta, SCREEN_WIDTH - 20, 24);
    lv_obj_align_to(settings_wifi_pass_ta, settings_wifi_ssid_ta, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 4);
    lv_textarea_set_placeholder_text(settings_wifi_pass_ta, "WiFi Password");
    lv_textarea_set_one_line(settings_wifi_pass_ta, true);
    lv_textarea_set_password_mode(settings_wifi_pass_ta, true);
    lv_obj_set_style_bg_color(settings_wifi_pass_ta, color_surface0(), LV_PART_MAIN);
    lv_obj_set_style_text_color(settings_wifi_pass_ta, color_text(), LV_PART_MAIN);
    lv_obj_add_event_cb(settings_wifi_pass_ta, ta_focus_cb, LV_EVENT_FOCUSED, nullptr);

    // [Connect] button
    settings_wifi_connect_btn = lv_btn_create(cont);
    lv_obj_set_size(settings_wifi_connect_btn, SCREEN_WIDTH - 20, 28);
    lv_obj_align_to(settings_wifi_connect_btn, settings_wifi_pass_ta, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 4);
    lv_obj_add_style(settings_wifi_connect_btn, &style_btn_primary, 0);
    lv_obj_add_event_cb(settings_wifi_connect_btn, wifi_connect_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* connect_lbl = lv_label_create(settings_wifi_connect_btn);
    lv_label_set_text(connect_lbl, "Connect & Save");
    lv_obj_center(connect_lbl);

    // Saved networks list
    lv_obj_t* saved_title = lv_label_create(cont);
    lv_label_set_text(saved_title, "Saved Networks:");
    lv_obj_add_style(saved_title, &style_small, 0);
    lv_obj_set_style_text_color(saved_title, color_overlay0(), LV_PART_MAIN);
    lv_obj_align_to(saved_title, settings_wifi_connect_btn, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 6);

    settings_saved_list_cont = lv_obj_create(cont);
    lv_obj_set_size(settings_saved_list_cont, SCREEN_WIDTH - 20, LV_SIZE_CONTENT);
    lv_obj_align_to(settings_saved_list_cont, saved_title, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 2);
    lv_obj_set_style_bg_color(settings_saved_list_cont, color_bg(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(settings_saved_list_cont, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(settings_saved_list_cont, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(settings_saved_list_cont, 2, LV_PART_MAIN);
    lv_obj_clear_flag(settings_saved_list_cont, LV_OBJ_FLAG_SCROLLABLE);
    refresh_saved_list();

    // ─── UGENT Server Section ─────────────────────────────────────────────
    lv_obj_t* ugent_title = lv_label_create(cont);
    lv_label_set_text(ugent_title, "UGENT Server");
    lv_obj_add_style(ugent_title, &style_title, 0);
    lv_obj_set_style_text_color(ugent_title, color_teal(), LV_PART_MAIN);
    lv_obj_align_to(ugent_title, settings_saved_list_cont, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 8);

    // Host
    settings_ugent_host_ta = lv_textarea_create(cont);
    lv_obj_set_size(settings_ugent_host_ta, SCREEN_WIDTH - 20, 24);
    lv_obj_align_to(settings_ugent_host_ta, ugent_title, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 4);
    lv_textarea_set_placeholder_text(settings_ugent_host_ta, "Host IP (e.g. 192.168.1.100)");
    lv_textarea_set_one_line(settings_ugent_host_ta, true);
    lv_obj_set_style_bg_color(settings_ugent_host_ta, color_surface0(), LV_PART_MAIN);
    lv_obj_set_style_text_color(settings_ugent_host_ta, color_text(), LV_PART_MAIN);
    lv_obj_add_event_cb(settings_ugent_host_ta, ta_focus_cb, LV_EVENT_FOCUSED, nullptr);
    if (nvs->hasUgentConfig()) {
        lv_textarea_set_text(settings_ugent_host_ta, nvs->getUgentHost().c_str());
    }

    // Port
    settings_ugent_port_ta = lv_textarea_create(cont);
    lv_obj_set_size(settings_ugent_port_ta, SCREEN_WIDTH - 20, 24);
    lv_obj_align_to(settings_ugent_port_ta, settings_ugent_host_ta, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 4);
    lv_textarea_set_placeholder_text(settings_ugent_port_ta, "Port (default: 8765)");
    lv_textarea_set_one_line(settings_ugent_port_ta, true);
    lv_obj_set_style_bg_color(settings_ugent_port_ta, color_surface0(), LV_PART_MAIN);
    lv_obj_set_style_text_color(settings_ugent_port_ta, color_text(), LV_PART_MAIN);
    lv_obj_add_event_cb(settings_ugent_port_ta, ta_focus_cb, LV_EVENT_FOCUSED, nullptr);
    {
        char portStr[8];
        snprintf(portStr, sizeof(portStr), "%d", nvs->getUgentPort());
        lv_textarea_set_text(settings_ugent_port_ta, portStr);
    }

    // API Key
    settings_ugent_apikey_ta = lv_textarea_create(cont);
    lv_obj_set_size(settings_ugent_apikey_ta, SCREEN_WIDTH - 20, 24);
    lv_obj_align_to(settings_ugent_apikey_ta, settings_ugent_port_ta, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 4);
    lv_textarea_set_placeholder_text(settings_ugent_apikey_ta, "API Key (optional)");
    lv_textarea_set_one_line(settings_ugent_apikey_ta, true);
    lv_textarea_set_password_mode(settings_ugent_apikey_ta, true);
    lv_obj_set_style_bg_color(settings_ugent_apikey_ta, color_surface0(), LV_PART_MAIN);
    lv_obj_set_style_text_color(settings_ugent_apikey_ta, color_text(), LV_PART_MAIN);
    lv_obj_add_event_cb(settings_ugent_apikey_ta, ta_focus_cb, LV_EVENT_FOCUSED, nullptr);
    if (nvs->hasUgentConfig()) {
        lv_textarea_set_text(settings_ugent_apikey_ta, nvs->getUgentApiKey().c_str());
    }

    // Test button
    settings_test_btn = lv_btn_create(cont);
    lv_obj_set_size(settings_test_btn, SCREEN_WIDTH - 20, 28);
    lv_obj_align_to(settings_test_btn, settings_ugent_apikey_ta, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 4);
    lv_obj_add_style(settings_test_btn, &style_btn_primary, 0);
    lv_obj_add_event_cb(settings_test_btn, test_conn_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* test_lbl = lv_label_create(settings_test_btn);
    lv_label_set_text(test_lbl, "Test Connection");
    lv_obj_center(test_lbl);

    // ─── Brightness ───────────────────────────────────────────────────────
    lv_obj_t* bright_title = lv_label_create(cont);
    lv_label_set_text(bright_title, "Brightness");
    lv_obj_add_style(bright_title, &style_body, 0);
    lv_obj_align_to(bright_title, settings_test_btn, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 10);

    settings_brightness_slider = lv_slider_create(cont);
    lv_obj_set_size(settings_brightness_slider, SCREEN_WIDTH - 20, 12);
    lv_obj_align_to(settings_brightness_slider, bright_title, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 4);
    lv_slider_set_range(settings_brightness_slider, 10, 255);
    lv_slider_set_value(settings_brightness_slider, nvs->getBrightness(), LV_ANIM_OFF);
    lv_obj_set_style_bg_color(settings_brightness_slider, color_surface1(), LV_PART_MAIN);
    lv_obj_set_style_bg_color(settings_brightness_slider, color_blue(), LV_PART_INDICATOR);
    lv_obj_add_event_cb(settings_brightness_slider, brightness_cb, LV_EVENT_VALUE_CHANGED, nullptr);

    // ─── Keyboard ─────────────────────────────────────────────────────────
    settings_kb = lv_keyboard_create(parent);
    lv_obj_set_size(settings_kb, SCREEN_WIDTH, 100);
    lv_obj_align(settings_kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(settings_kb, color_surface0(), LV_PART_MAIN);
    lv_obj_set_style_text_color(settings_kb, color_text(), LV_PART_ITEMS);
    lv_obj_add_flag(settings_kb, LV_OBJ_FLAG_HIDDEN);

    // Show/hide keyboard on textarea focus
    lv_obj_add_event_cb(settings_wifi_ssid_ta, [](lv_event_t*){ show_kb(); }, LV_EVENT_FOCUSED, nullptr);
    lv_obj_add_event_cb(settings_wifi_pass_ta, [](lv_event_t*){ show_kb(); }, LV_EVENT_FOCUSED, nullptr);
    lv_obj_add_event_cb(settings_ugent_host_ta, [](lv_event_t*){ show_kb(); }, LV_EVENT_FOCUSED, nullptr);
    lv_obj_add_event_cb(settings_ugent_port_ta, [](lv_event_t*){ show_kb(); }, LV_EVENT_FOCUSED, nullptr);
    lv_obj_add_event_cb(settings_ugent_apikey_ta, [](lv_event_t*){ show_kb(); }, LV_EVENT_FOCUSED, nullptr);

    // Hide keyboard on textarea ready (enter key)
    lv_obj_add_event_cb(settings_wifi_ssid_ta, [](lv_event_t*){ hide_kb(); }, LV_EVENT_READY, nullptr);
    lv_obj_add_event_cb(settings_wifi_pass_ta, [](lv_event_t*){ hide_kb(); }, LV_EVENT_READY, nullptr);
    lv_obj_add_event_cb(settings_ugent_host_ta, [](lv_event_t*){ hide_kb(); }, LV_EVENT_READY, nullptr);
    lv_obj_add_event_cb(settings_ugent_port_ta, [](lv_event_t*){ hide_kb(); }, LV_EVENT_READY, nullptr);
    lv_obj_add_event_cb(settings_ugent_apikey_ta, [](lv_event_t*){ hide_kb(); }, LV_EVENT_READY, nullptr);
}

#endif // SCREEN_SETTINGS_H
