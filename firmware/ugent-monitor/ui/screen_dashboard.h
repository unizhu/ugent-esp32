/**
 * UGENT ESP32 Monitor — Dashboard Screen
 *
 * Main status display: agent state, task counts, active tasks, alerts.
 */

#ifndef SCREEN_DASHBOARD_H
#define SCREEN_DASHBOARD_H

#include <lvgl.h>
#include "ui_theme.h"
#include "../config.h"
#include "../ugent_client.h"

// ─── Widget References ────────────────────────────────────────────────────────

static lv_obj_t* dash_status_label   = nullptr;
static lv_obj_t* dash_version_label  = nullptr;
static lv_obj_t* dash_sessions_val   = nullptr;
static lv_obj_t* dash_tasks_run_val  = nullptr;
static lv_obj_t* dash_tasks_done_val = nullptr;
static lv_obj_t* dash_memory_val     = nullptr;
static lv_obj_t* dash_cron_val       = nullptr;
static lv_obj_t* dash_skills_val     = nullptr;
static lv_obj_t* dash_task_list      = nullptr;
static lv_obj_t* dash_alert_panel    = nullptr;
static lv_obj_t* dash_wifi_icon      = nullptr;
static lv_obj_t* dash_conn_icon      = nullptr;

// ─── Create ───────────────────────────────────────────────────────────────────

inline void screen_dashboard_create(lv_obj_t* parent) {
    // Main container with padding
    lv_obj_t* cont = lv_obj_create(parent);
    lv_obj_set_size(cont, SCREEN_WIDTH - 8, SCREEN_HEIGHT - 44);
    lv_obj_align(cont, LV_ALIGN_TOP_MID, 0, 2);
    lv_obj_set_style_bg_color(cont, color_bg(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(cont, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(cont, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(cont, 4, LV_PART_MAIN);
    lv_obj_set_style_pad_gap(cont, 2, LV_PART_MAIN);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);

    // ─── Header Row ───────────────────────────────────────────────────────
    lv_obj_t* header = lv_obj_create(cont);
    lv_obj_set_size(header, SCREEN_WIDTH - 20, 24);
    lv_obj_align(header, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(header, color_surface0(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(header, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(header, 4, LV_PART_MAIN);
    lv_obj_set_style_border_width(header, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(header, 2, LV_PART_MAIN);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    // Title
    lv_obj_t* title = lv_label_create(header);
    lv_label_set_text(title, "UGENT Monitor");
    lv_obj_add_style(title, &style_title, 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 4, 0);

    // WiFi icon (right side)
    dash_wifi_icon = lv_label_create(header);
    lv_label_set_text(dash_wifi_icon, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_color(dash_wifi_icon, color_overlay0(), LV_PART_MAIN);
    lv_obj_align(dash_wifi_icon, LV_ALIGN_RIGHT_MID, -4, 0);

    // Connection status dot
    dash_conn_icon = lv_label_create(header);
    lv_label_set_text(dash_conn_icon, LV_SYMBOL_CIRCLE);
    lv_obj_set_style_text_color(dash_conn_icon, color_red(), LV_PART_MAIN);
    lv_obj_align(dash_conn_icon, LV_ALIGN_RIGHT_MID, -24, 0);

    // ─── Status Bar ───────────────────────────────────────────────────────
    lv_obj_t* status_bar = lv_obj_create(cont);
    lv_obj_set_size(status_bar, SCREEN_WIDTH - 20, 22);
    lv_obj_align_to(status_bar, header, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 4);
    lv_obj_set_style_bg_color(status_bar, color_surface1(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(status_bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(status_bar, 4, LV_PART_MAIN);
    lv_obj_set_style_border_width(status_bar, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(status_bar, 2, LV_PART_MAIN);
    lv_obj_clear_flag(status_bar, LV_OBJ_FLAG_SCROLLABLE);

    dash_status_label = lv_label_create(status_bar);
    lv_label_set_text(dash_status_label, "IDLE");
    lv_obj_add_style(dash_status_label, &style_status_pend, 0);
    lv_obj_align(dash_status_label, LV_ALIGN_LEFT_MID, 4, 0);

    dash_version_label = lv_label_create(status_bar);
    lv_label_set_text(dash_version_label, "v--");
    lv_obj_add_style(dash_version_label, &style_small, 0);
    lv_obj_align(dash_version_label, LV_ALIGN_RIGHT_MID, -4, 0);

    // ─── Stats Grid (2 columns) ──────────────────────────────────────────
    lv_obj_t* grid = lv_obj_create(cont);
    lv_obj_set_size(grid, SCREEN_WIDTH - 20, 70);
    lv_obj_align_to(grid, status_bar, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 4);
    lv_obj_set_style_bg_color(grid, color_bg(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(grid, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(grid, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(grid, 0, LV_PART_MAIN);
    lv_obj_clear_flag(grid, LV_OBJ_FLAG_SCROLLABLE);

    static lv_coord_t col_dsc[] = {152, 152, LV_COORD_MAX};
    static lv_coord_t row_dsc[] = {22, 22, 22, LV_COORD_MAX};
    lv_obj_set_layout(grid, LV_LAYOUT_GRID);
    lv_obj_set_grid_dsc_array(grid, col_dsc, row_dsc);
    lv_obj_set_style_pad_column(grid, 4, LV_PART_MAIN);
    lv_obj_set_style_pad_row(grid, 2, LV_PART_MAIN);

    // Row 1
    auto stat_cell = [&](const char* label_text, lv_obj_t** val_label,
                         uint8_t col, uint8_t row) {
        lv_obj_t* cell = lv_obj_create(grid);
        lv_obj_set_style_bg_color(cell, color_surface0(), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(cell, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_radius(cell, 4, LV_PART_MAIN);
        lv_obj_set_style_border_width(cell, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(cell, 2, LV_PART_MAIN);
        lv_obj_set_grid_cell(cell, LV_GRID_ALIGN_STRETCH, col, 1,
                             LV_GRID_ALIGN_STRETCH, row, 1);
        lv_obj_clear_flag(cell, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* lbl = lv_label_create(cell);
        lv_label_set_text(lbl, label_text);
        lv_obj_add_style(lbl, &style_small, 0);
        lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 4, 0);

        *val_label = lv_label_create(cell);
        lv_label_set_text(*val_label, "--");
        lv_obj_add_style(*val_label, &style_body, 0);
        lv_obj_align(*val_label, LV_ALIGN_RIGHT_MID, -4, 0);
    };

    stat_cell("Sessions", &dash_sessions_val,   0, 0);
    stat_cell("Running",  &dash_tasks_run_val,   1, 0);
    stat_cell("Done",     &dash_tasks_done_val,  0, 1);
    stat_cell("Memory",   &dash_memory_val,       1, 1);
    stat_cell("Cron",     &dash_cron_val,         0, 2);
    stat_cell("Skills",   &dash_skills_val,       1, 2);

    // ─── Active Tasks List ────────────────────────────────────────────────
    lv_obj_t* task_header = lv_label_create(cont);
    lv_label_set_text(task_header, "Active Tasks");
    lv_obj_add_style(task_header, &style_small, 0);
    lv_obj_set_style_text_color(task_header, color_overlay0(), LV_PART_MAIN);
    lv_obj_align_to(task_header, grid, LV_ALIGN_OUT_BOTTOM_LEFT, 4, 4);

    dash_task_list = lv_obj_create(cont);
    lv_obj_set_size(dash_task_list, SCREEN_WIDTH - 20, 52);
    lv_obj_align_to(dash_task_list, task_header, LV_ALIGN_OUT_BOTTOM_LEFT, -4, 2);
    lv_obj_set_style_bg_color(dash_task_list, color_surface0(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(dash_task_list, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(dash_task_list, 4, LV_PART_MAIN);
    lv_obj_set_style_border_width(dash_task_list, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(dash_task_list, 4, LV_PART_MAIN);

    // Placeholder
    lv_obj_t* placeholder = lv_label_create(dash_task_list);
    lv_label_set_text(placeholder, "No tasks");
    lv_obj_add_style(placeholder, &style_small, 0);
    lv_obj_set_style_text_color(placeholder, color_overlay0(), LV_PART_MAIN);
    lv_obj_align(placeholder, LV_ALIGN_CENTER, 0, 0);
}

// ─── Update ───────────────────────────────────────────────────────────────────

inline void screen_dashboard_update(UgentClient* client) {
    if (!client) return;
    const UgentStatus& s = client->getStatus();

    // Status bar — use set_style instead of add_style to avoid accumulation
    if (s.agentRunning) {
        lv_label_set_text(dash_status_label, "RUNNING");
        lv_obj_set_style_text_color(dash_status_label, color_green(), LV_PART_MAIN);
    } else {
        lv_label_set_text(dash_status_label, "IDLE");
        lv_obj_set_style_text_color(dash_status_label, color_overlay0(), LV_PART_MAIN);
    }

    // Version
    lv_label_set_text_fmt(dash_version_label, "v%s", s.version);

    // Stats
    lv_label_set_text_fmt(dash_sessions_val,  "%d", s.sessionsActive);
    lv_label_set_text_fmt(dash_tasks_run_val,  "%d", s.tasksRunning);
    lv_label_set_text_fmt(dash_tasks_done_val, "%d", s.tasksCompleted);
    lv_label_set_text_fmt(dash_memory_val,     "%d", s.memoryEntries);
    lv_label_set_text_fmt(dash_cron_val,       "%d", s.cronJobs);
    lv_label_set_text_fmt(dash_skills_val,     "%d", s.skillsLoaded);

    // Connection indicator
    lv_obj_set_style_text_color(dash_conn_icon,
        client->isConnected() ? color_green() : color_red(), LV_PART_MAIN);

    // Active tasks list — rebuild
    lv_obj_clean(dash_task_list);
    if (s.taskCount == 0) {
        lv_obj_t* ph = lv_label_create(dash_task_list);
        lv_label_set_text(ph, "No active tasks");
        lv_obj_add_style(ph, &style_small, 0);
        lv_obj_set_style_text_color(ph, color_overlay0(), LV_PART_MAIN);
        lv_obj_align(ph, LV_ALIGN_CENTER, 0, 0);
    } else {
        for (uint8_t i = 0; i < s.taskCount && i < 4; i++) {
            lv_obj_t* row = lv_label_create(dash_task_list);
            const char* statusIcon = "?";
            switch (s.tasks[i].status) {
                case TaskStatus::IN_PROGRESS: statusIcon = ">"; break;
                case TaskStatus::COMPLETED:   statusIcon = "+"; break;
                case TaskStatus::FAILED:      statusIcon = "x"; break;
                case TaskStatus::PENDING:     statusIcon = "-"; break;
                default: break;
            }
            lv_label_set_text_fmt(row, "%s %s", statusIcon, s.tasks[i].name);
            lv_obj_add_style(row, &style_small, LV_PART_MAIN);
            lv_obj_set_width(row, SCREEN_WIDTH - 36);
            lv_obj_align(row, LV_ALIGN_TOP_LEFT, 0, i * 12);
        }
    }
}

#endif // SCREEN_DASHBOARD_H
