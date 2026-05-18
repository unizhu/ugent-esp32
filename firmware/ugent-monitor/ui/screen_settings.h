/**
 * UGENT ESP32 Monitor — Settings Screen
 *
 * WiFi setup, UGENT server configuration, brightness control.
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
static lv_obj_t* settings_smartcfg_btn = nullptr;
static lv_obj_t* settings_ugent_host_ta = nullptr;
static lv_obj_t* settings_ugent_port_ta = nullptr;
static lv_obj_t* settings_ugent_apikey_ta = nullptr;
static lv_obj_t* settings_test_btn = nullptr;
static lv_obj_t* settings_brightness_slider = nullptr;
static lv_obj_t* settings_feedback = nullptr;
static lv_obj_t* settings_kb = nullptr;

// ─── Keyboard handling ─────────────────────────────────────────────────────────

static lv_obj_t* settings_active_ta = nullptr;

static void ta_focus_cb(lv_event_t* e) {
    settings_active_ta = (lv_obj_t*)lv_event_get_target(e);
    if (settings_kb) {
        lv_keyboard_set_textarea(settings_kb, settings_active_ta);
    }
}

// ─── Button Callbacks ──────────────────────────────────────────────────────────

static void wifi_connect_cb(lv_event_t* e) {
    if (!settings_wifi_ || !settings_nvs_) return;

    const char* ssid = lv_textarea_get_text(settings_wifi_ssid_ta);
    const char* pass = lv_textarea_get_text(settings_wifi_pass_ta);

    if (strlen(ssid) == 0) {
        lv_label_set_text(settings_feedback, "Enter SSID");
        lv_obj_set_style_text_color(settings_feedback, color_yellow(), LV_PART_MAIN);
        return;
    }

    lv_label_set_text(settings_feedback, "Connecting...");
    lv_obj_set_style_text_color(settings_feedback, color_blue(), LV_PART_MAIN);

    // Save and connect (runs in LVGL task context, may block briefly)
    settings_nvs_->setWifiSsid(ssid);
    settings_nvs_->setWifiPass(pass);

    if (settings_wifi_->connect(ssid, pass)) {
        lv_label_set_text(settings_feedback, "Connected!");
        lv_obj_set_style_text_color(settings_feedback, color_green(), LV_PART_MAIN);
        lv_label_set_text_fmt(settings_wifi_status,
            "%s (%s)", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
    } else {
        lv_label_set_text(settings_feedback, "Failed!");
        lv_obj_set_style_text_color(settings_feedback, color_red(), LV_PART_MAIN);
    }
}

static void smartconfig_cb(lv_event_t* e) {
    if (!settings_wifi_) return;
    settings_wifi_->startSmartConfig();
    lv_label_set_text(settings_feedback, "Waiting SmartConfig...");
    lv_obj_set_style_text_color(settings_feedback, color_yellow(), LV_PART_MAIN);
}

static void test_conn_cb(lv_event_t* e) {
    if (!settings_nvs_ || !settings_ugent_) return;

    // Save values first
    settings_nvs_->setUgentHost(lv_textarea_get_text(settings_ugent_host_ta));
    const char* portStr = lv_textarea_get_text(settings_ugent_port_ta);
    settings_nvs_->setUgentPort((uint16_t)atoi(portStr));
    settings_nvs_->setUgentApiKey(lv_textarea_get_text(settings_ugent_apikey_ta));

    lv_label_set_text(settings_feedback, "Testing...");
    lv_obj_set_style_text_color(settings_feedback, color_blue(), LV_PART_MAIN);

    if (settings_ugent_->testConnection()) {
        lv_label_set_text(settings_feedback, "Connected to UGENT!");
        lv_obj_set_style_text_color(settings_feedback, color_green(), LV_PART_MAIN);
    } else {
        lv_label_set_text(settings_feedback, "Connection failed!");
        lv_obj_set_style_text_color(settings_feedback, color_red(), LV_PART_MAIN);
    }
}

static void brightness_cb(lv_event_t* e) {
    if (!settings_nvs_) return;
    int val = lv_slider_get_value(settings_brightness_slider);
    settings_nvs_->setBrightness((uint8_t)val);
    // Update PWM — will be applied in main loop
}

// ─── Create ────────────────────────────────────────────────────────────────────

inline void screen_settings_create(lv_obj_t* parent, NvsStorage* nvs,
                                   WifiManager* wifi, UgentClient* ugent) {
    settings_nvs_ = nvs;
    settings_wifi_ = wifi;
    settings_ugent_ = ugent;

    // Main container — scrollable since settings are long
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
    lv_label_set_text(settings_wifi_status, "Not connected");
    lv_obj_add_style(settings_wifi_status, &style_small, 0);
    lv_obj_align_to(settings_wifi_status, wifi_title, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 2);

    // SSID input
    settings_wifi_ssid_ta = lv_textarea_create(cont);
    lv_obj_set_size(settings_wifi_ssid_ta, SCREEN_WIDTH - 20, 24);
    lv_obj_align_to(settings_wifi_ssid_ta, settings_wifi_status, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 4);
    lv_textarea_set_placeholder_text(settings_wifi_ssid_ta, "WiFi SSID");
    lv_textarea_set_one_line(settings_wifi_ssid_ta, true);
    lv_obj_set_style_bg_color(settings_wifi_ssid_ta, color_surface0(), LV_PART_MAIN);
    lv_obj_set_style_text_color(settings_wifi_ssid_ta, color_text(), LV_PART_MAIN);
    lv_obj_add_event_cb(settings_wifi_ssid_ta, ta_focus_cb, LV_EVENT_FOCUSED, nullptr);

    // Pre-fill from NVS
    if (nvs->hasWifiCredentials()) {
        lv_textarea_set_text(settings_wifi_ssid_ta, nvs->getWifiSsid().c_str());
    }

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

    // WiFi buttons row
    settings_wifi_connect_btn = lv_btn_create(cont);
    lv_obj_set_size(settings_wifi_connect_btn, 140, 28);
    lv_obj_align_to(settings_wifi_connect_btn, settings_wifi_pass_ta, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 4);
    lv_obj_add_style(settings_wifi_connect_btn, &style_btn_primary, 0);
    lv_obj_add_event_cb(settings_wifi_connect_btn, wifi_connect_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* connect_lbl = lv_label_create(settings_wifi_connect_btn);
    lv_label_set_text(connect_lbl, "Connect");
    lv_obj_center(connect_lbl);

    settings_smartcfg_btn = lv_btn_create(cont);
    lv_obj_set_size(settings_smartcfg_btn, 140, 28);
    lv_obj_align_to(settings_smartcfg_btn, settings_wifi_pass_ta, LV_ALIGN_OUT_BOTTOM_RIGHT, 0, 4);
    lv_obj_set_style_bg_color(settings_smartcfg_btn, color_surface1(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(settings_smartcfg_btn, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_text_color(settings_smartcfg_btn, color_text(), LV_PART_MAIN);
    lv_obj_set_style_radius(settings_smartcfg_btn, 6, LV_PART_MAIN);
    lv_obj_add_event_cb(settings_smartcfg_btn, smartconfig_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* sc_lbl = lv_label_create(settings_smartcfg_btn);
    lv_label_set_text(sc_lbl, "SmartConfig");
    lv_obj_center(sc_lbl);

    // ─── UGENT Server Section ─────────────────────────────────────────────
    lv_obj_t* ugent_title = lv_label_create(cont);
    lv_label_set_text(ugent_title, "UGENT Server");
    lv_obj_add_style(ugent_title, &style_title, 0);
    lv_obj_set_style_text_color(ugent_title, color_teal(), LV_PART_MAIN);
    lv_obj_align_to(ugent_title, settings_wifi_connect_btn, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 10);

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
    lv_slider_set_range(settings_brightness_slider, 10, 100);
    lv_slider_set_value(settings_brightness_slider, nvs->getBrightness(), LV_ANIM_OFF);
    lv_obj_set_style_bg_color(settings_brightness_slider, color_surface1(), LV_PART_MAIN);
    lv_obj_set_style_bg_color(settings_brightness_slider, color_blue(), LV_PART_INDICATOR);
    lv_obj_add_event_cb(settings_brightness_slider, brightness_cb, LV_EVENT_VALUE_CHANGED, nullptr);

    // ─── Keyboard ─────────────────────────────────────────────────────────
    settings_kb = lv_keyboard_create(parent);  // On main parent, not scrollable cont
    lv_obj_set_size(settings_kb, SCREEN_WIDTH, 100);
    lv_obj_align(settings_kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(settings_kb, color_surface0(), LV_PART_MAIN);
    lv_obj_set_style_text_color(settings_kb, color_text(), LV_PART_ITEMS);
    lv_obj_add_flag(settings_kb, LV_OBJ_FLAG_HIDDEN);

    // Show/hide keyboard on textarea focus
    auto kb_show_cb = [](lv_event_t* e) {
        lv_obj_clear_flag(settings_kb, LV_OBJ_FLAG_HIDDEN);
    };
    auto kb_hide_cb = [](lv_event_t* e) {
        lv_obj_add_flag(settings_kb, LV_OBJ_FLAG_HIDDEN);
    };

    lv_obj_add_event_cb(settings_wifi_ssid_ta, kb_show_cb, LV_EVENT_FOCUSED, nullptr);
    lv_obj_add_event_cb(settings_wifi_pass_ta, kb_show_cb, LV_EVENT_FOCUSED, nullptr);
    lv_obj_add_event_cb(settings_ugent_host_ta, kb_show_cb, LV_EVENT_FOCUSED, nullptr);
    lv_obj_add_event_cb(settings_ugent_port_ta, kb_show_cb, LV_EVENT_FOCUSED, nullptr);
    lv_obj_add_event_cb(settings_ugent_apikey_ta, kb_show_cb, LV_EVENT_FOCUSED, nullptr);

    // Hide keyboard on confirm (LV_EVENT_READY from textarea)
    lv_obj_add_event_cb(settings_wifi_ssid_ta, kb_hide_cb, LV_EVENT_READY, nullptr);
    lv_obj_add_event_cb(settings_wifi_pass_ta, kb_hide_cb, LV_EVENT_READY, nullptr);
    lv_obj_add_event_cb(settings_ugent_host_ta, kb_hide_cb, LV_EVENT_READY, nullptr);
    lv_obj_add_event_cb(settings_ugent_port_ta, kb_hide_cb, LV_EVENT_READY, nullptr);
    lv_obj_add_event_cb(settings_ugent_apikey_ta, kb_hide_cb, LV_EVENT_READY, nullptr);

    // Update wifi status if already connected
    if (wifi && wifi->isConnected()) {
        lv_label_set_text_fmt(settings_wifi_status,
            "%s (%s)", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
    }
}

#endif // SCREEN_SETTINGS_H
