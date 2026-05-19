/**
 * UGENT ESP32 Monitor — UI Theme
 *
 * Catppuccin Mocha-inspired dark theme for 320x240 display.
 */

#ifndef UI_THEME_H
#define UI_THEME_H

#include <lvgl.h>
#include "config.h"

// ─── Color Helpers ────────────────────────────────────────────────────────────
// VIVID palette — Catppuccin Mocha was too muted for 2.8" 16-bit TFT.
// These are bright, saturated colors that are clearly visible.
// TODO: tune to taste once we confirm colors render correctly.

static inline lv_color_t color_bg()          { return lv_color_hex(0x0D1117); }  // Deep dark (GitHub dark)
static inline lv_color_t color_surface0()    { return lv_color_hex(0x161B22); }  // Dark panel
static inline lv_color_t color_surface1()    { return lv_color_hex(0x21262D); }  // Elevated panel
static inline lv_color_t color_surface2()    { return lv_color_hex(0x30363D); }  // Border
static inline lv_color_t color_overlay0()    { return lv_color_hex(0x8B949E); }  // Muted text
static inline lv_color_t color_text()        { return lv_color_hex(0xF0F6FC); }  // Bright white text
static inline lv_color_t color_subtext()     { return lv_color_hex(0xC9D1D9); }  // Secondary text
static inline lv_color_t color_blue()        { return lv_color_hex(0x58A6FF); }  // Vivid blue
static inline lv_color_t color_green()       { return lv_color_hex(0x3FB950); }  // Vivid green
static inline lv_color_t color_red()         { return lv_color_hex(0xFF7B72); }  // Vivid red
static inline lv_color_t color_yellow()      { return lv_color_hex(0xE3B341); }  // Vivid yellow
static inline lv_color_t color_mauve()       { return lv_color_hex(0xD2A8FF); }  // Vivid purple
static inline lv_color_t color_teal()        { return lv_color_hex(0x39D353); }  // Bright teal
static inline lv_color_t color_peach()       { return lv_color_hex(0xFFA657); }  // Vivid orange

// ─── Style Instances ──────────────────────────────────────────────────────────

// Global styles — initialized once in theme_init()
static lv_style_t style_scr;         // Screen background
static lv_style_t style_card;        // Card/panel container
static lv_style_t style_title;       // Screen title label
static lv_style_t style_body;        // Body text
static lv_style_t style_small;       // Small text
static lv_style_t style_status_run;  // Running status badge
static lv_style_t style_status_done; // Completed status badge
static lv_style_t style_status_fail; // Failed status badge
static lv_style_t style_status_pend; // Pending status badge
static lv_style_t style_btn_primary; // Primary button
static lv_style_t style_btn_danger;  // Danger/reject button
static lv_style_t style_tab_active;  // Active tab button
static lv_style_t style_tab_inactive;// Inactive tab button

static inline void theme_init() {
    // Screen background
    lv_style_init(&style_scr);
    lv_style_set_bg_color(&style_scr, color_bg());
    lv_style_set_bg_opa(&style_scr, LV_OPA_COVER);

    // Card/panel
    lv_style_init(&style_card);
    lv_style_set_bg_color(&style_card, color_surface0());
    lv_style_set_bg_opa(&style_card, LV_OPA_COVER);
    lv_style_set_radius(&style_card, 8);
    lv_style_set_pad_all(&style_card, 8);
    lv_style_set_pad_gap(&style_card, 4);
    lv_style_set_border_width(&style_card, 0);

    // Title
    lv_style_init(&style_title);
    lv_style_set_text_color(&style_title, color_text());
    lv_style_set_text_font(&style_title, &lv_font_montserrat_16);

    // Body
    lv_style_init(&style_body);
    lv_style_set_text_color(&style_body, color_text());
    lv_style_set_text_font(&style_body, &lv_font_montserrat_12);

    // Small text
    lv_style_init(&style_small);
    lv_style_set_text_color(&style_small, color_subtext());
    lv_style_set_text_font(&style_small, &lv_font_montserrat_10);

    // Status badges
    lv_style_init(&style_status_run);
    lv_style_set_text_color(&style_status_run, color_green());
    lv_style_set_text_font(&style_status_run, &lv_font_montserrat_12);
    lv_style_set_bg_color(&style_status_run, color_bg());
    lv_style_set_bg_opa(&style_status_run, LV_OPA_COVER);
    lv_style_set_radius(&style_status_run, 4);
    lv_style_set_pad_hor(&style_status_run, 6);
    lv_style_set_pad_ver(&style_status_run, 2);

    lv_style_init(&style_status_done);
    lv_style_set_text_color(&style_status_done, color_blue());
    lv_style_set_text_font(&style_status_done, &lv_font_montserrat_12);
    lv_style_set_bg_color(&style_status_done, color_bg());
    lv_style_set_bg_opa(&style_status_done, LV_OPA_COVER);
    lv_style_set_radius(&style_status_done, 4);
    lv_style_set_pad_hor(&style_status_done, 6);
    lv_style_set_pad_ver(&style_status_done, 2);

    lv_style_init(&style_status_fail);
    lv_style_set_text_color(&style_status_fail, color_red());
    lv_style_set_text_font(&style_status_fail, &lv_font_montserrat_12);
    lv_style_set_bg_color(&style_status_fail, color_bg());
    lv_style_set_bg_opa(&style_status_fail, LV_OPA_COVER);
    lv_style_set_radius(&style_status_fail, 4);
    lv_style_set_pad_hor(&style_status_fail, 6);
    lv_style_set_pad_ver(&style_status_fail, 2);

    lv_style_init(&style_status_pend);
    lv_style_set_text_color(&style_status_pend, color_overlay0());
    lv_style_set_text_font(&style_status_pend, &lv_font_montserrat_12);
    lv_style_set_bg_color(&style_status_pend, color_bg());
    lv_style_set_bg_opa(&style_status_pend, LV_OPA_COVER);
    lv_style_set_radius(&style_status_pend, 4);
    lv_style_set_pad_hor(&style_status_pend, 6);
    lv_style_set_pad_ver(&style_status_pend, 2);

    // Primary button
    lv_style_init(&style_btn_primary);
    lv_style_set_bg_color(&style_btn_primary, color_blue());
    lv_style_set_bg_opa(&style_btn_primary, LV_OPA_COVER);
    lv_style_set_text_color(&style_btn_primary, color_bg());
    lv_style_set_text_font(&style_btn_primary, &lv_font_montserrat_14);
    lv_style_set_radius(&style_btn_primary, 6);
    lv_style_set_pad_hor(&style_btn_primary, 16);
    lv_style_set_pad_ver(&style_btn_primary, 8);

    // Danger button
    lv_style_init(&style_btn_danger);
    lv_style_set_bg_color(&style_btn_danger, color_red());
    lv_style_set_bg_opa(&style_btn_danger, LV_OPA_COVER);
    lv_style_set_text_color(&style_btn_danger, color_bg());
    lv_style_set_text_font(&style_btn_danger, &lv_font_montserrat_14);
    lv_style_set_radius(&style_btn_danger, 6);
    lv_style_set_pad_hor(&style_btn_danger, 16);
    lv_style_set_pad_ver(&style_btn_danger, 8);

    // Tab buttons
    lv_style_init(&style_tab_active);
    lv_style_set_bg_color(&style_tab_active, color_surface1());
    lv_style_set_bg_opa(&style_tab_active, LV_OPA_COVER);
    lv_style_set_text_color(&style_tab_active, color_blue());
    lv_style_set_text_font(&style_tab_active, &lv_font_montserrat_12);

    lv_style_init(&style_tab_inactive);
    lv_style_set_bg_color(&style_tab_inactive, color_bg());
    lv_style_set_bg_opa(&style_tab_inactive, LV_OPA_COVER);
    lv_style_set_text_color(&style_tab_inactive, color_overlay0());
    lv_style_set_text_font(&style_tab_inactive, &lv_font_montserrat_12);
}

#endif // UI_THEME_H
