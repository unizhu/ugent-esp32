/**
 * UGENT ESP32 Monitor — Task Monitor Screen
 *
 * Detailed task list with status badges and progress.
 */

#ifndef SCREEN_TASKS_H
#define SCREEN_TASKS_H

#include <lvgl.h>
#include "ui_theme.h"
#include "../config.h"
#include "../ugent_client.h"

static lv_obj_t* tasks_list_cont = nullptr;
static lv_obj_t* tasks_summary_label = nullptr;
static lv_obj_t* tasks_swarm_label = nullptr;

inline void screen_tasks_create(lv_obj_t* parent) {
    // Main container
    lv_obj_t* cont = lv_obj_create(parent);
    lv_obj_set_size(cont, SCREEN_WIDTH - 8, SCREEN_HEIGHT - 44);
    lv_obj_align(cont, LV_ALIGN_TOP_MID, 0, 2);
    lv_obj_set_style_bg_color(cont, color_bg(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(cont, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(cont, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(cont, 4, LV_PART_MAIN);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);

    // Header
    lv_obj_t* header = lv_label_create(cont);
    lv_label_set_text(header, "Task Monitor");
    lv_obj_add_style(header, &style_title, 0);
    lv_obj_align(header, LV_ALIGN_TOP_LEFT, 4, 0);

    // Summary
    tasks_summary_label = lv_label_create(cont);
    lv_label_set_text(tasks_summary_label, "0 running / 0 done / 0 failed");
    lv_obj_add_style(tasks_summary_label, &style_small, 0);
    lv_obj_set_style_text_color(tasks_summary_label, color_subtext(), LV_PART_MAIN);
    lv_obj_align_to(tasks_summary_label, header, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 2);

    // Swarm status
    tasks_swarm_label = lv_label_create(cont);
    lv_label_set_text(tasks_swarm_label, "Swarm: off");
    lv_obj_add_style(tasks_swarm_label, &style_small, 0);
    lv_obj_set_style_text_color(tasks_swarm_label, color_overlay0(), LV_PART_MAIN);
    lv_obj_align_to(tasks_swarm_label, tasks_summary_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 2);

    // Task list (scrollable)
    tasks_list_cont = lv_obj_create(cont);
    lv_obj_set_size(tasks_list_cont, SCREEN_WIDTH - 20, SCREEN_HEIGHT - 110);
    lv_obj_align_to(tasks_list_cont, tasks_swarm_label, LV_ALIGN_OUT_BOTTOM_LEFT, -4, 6);
    lv_obj_set_style_bg_color(tasks_list_cont, color_surface0(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(tasks_list_cont, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(tasks_list_cont, 4, LV_PART_MAIN);
    lv_obj_set_style_border_width(tasks_list_cont, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(tasks_list_cont, 4, LV_PART_MAIN);
    lv_obj_set_flex_flow(tasks_list_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(tasks_list_cont, 4, LV_PART_MAIN);
    lv_obj_add_flag(tasks_list_cont, LV_OBJ_FLAG_SCROLLABLE);
}

inline void screen_tasks_update(UgentClient* client) {
    if (!client) return;
    const UgentStatus& s = client->getStatus();

    // Summary
    lv_label_set_text_fmt(tasks_summary_label,
        "%d running / %d done / %d failed",
        s.tasksRunning, s.tasksCompleted, s.tasksFailed);

    // Swarm
    if (s.swarmActive) {
        lv_label_set_text(tasks_swarm_label, "Swarm: active");
        lv_obj_set_style_text_color(tasks_swarm_label, color_green(), LV_PART_MAIN);
    } else {
        lv_label_set_text(tasks_swarm_label, "Swarm: off");
        lv_obj_set_style_text_color(tasks_swarm_label, color_overlay0(), LV_PART_MAIN);
    }

    // Clear and rebuild task list
    lv_obj_clean(tasks_list_cont);

    if (s.taskCount == 0) {
        lv_obj_t* ph = lv_label_create(tasks_list_cont);
        lv_label_set_text(ph, "No tasks");
        lv_obj_add_style(ph, &style_small, 0);
        lv_obj_set_style_text_color(ph, color_overlay0(), LV_PART_MAIN);
        return;
    }

    for (uint8_t i = 0; i < s.taskCount; i++) {
        const TaskInfo& t = s.tasks[i];

        // Task card
        lv_obj_t* card = lv_obj_create(tasks_list_cont);
        lv_obj_set_size(card, SCREEN_WIDTH - 36, 32);
        lv_obj_set_style_bg_color(card, color_bg(), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(card, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_radius(card, 4, LV_PART_MAIN);
        lv_obj_set_style_border_width(card, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(card, 2, LV_PART_MAIN);
        lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

        // Status icon + task name
        lv_obj_t* name = lv_label_create(card);
        const char* icon = "?";
        lv_color_t iconColor = color_overlay0();
        switch (t.status) {
            case TaskStatus::IN_PROGRESS:
                icon = ">"; iconColor = color_green(); break;
            case TaskStatus::COMPLETED:
                icon = "+"; iconColor = color_blue(); break;
            case TaskStatus::FAILED:
                icon = "x"; iconColor = color_red(); break;
            case TaskStatus::PENDING:
                icon = "-"; iconColor = color_overlay0(); break;
            case TaskStatus::CANCELLED:
                icon = "~"; iconColor = color_overlay0(); break;
            case TaskStatus::SKIPPED:
                icon = ">>"; iconColor = color_yellow(); break;
            default: break;
        }
        lv_label_set_text_fmt(name, "%s %s", icon, t.name);
        lv_obj_set_style_text_color(name, iconColor, LV_PART_MAIN);
        lv_obj_set_style_text_font(name, &lv_font_montserrat_12, LV_PART_MAIN);
        lv_obj_set_width(name, SCREEN_WIDTH - 80);
        lv_obj_align(name, LV_ALIGN_LEFT_MID, 2, 0);

        // Progress (if in_progress)
        if (t.status == TaskStatus::IN_PROGRESS && t.progress > 0) {
            lv_obj_t* pct = lv_label_create(card);
            lv_label_set_text_fmt(pct, "%d%%", t.progress);
            lv_obj_add_style(pct, &style_small, 0);
            lv_obj_set_style_text_color(pct, color_green(), LV_PART_MAIN);
            lv_obj_align(pct, LV_ALIGN_RIGHT_MID, -2, 0);
        }

        // Status text
        const char* statusText = "unknown";
        switch (t.status) {
            case TaskStatus::PENDING:     statusText = "pend"; break;
            case TaskStatus::IN_PROGRESS: statusText = "run";  break;
            case TaskStatus::COMPLETED:   statusText = "done"; break;
            case TaskStatus::FAILED:      statusText = "fail"; break;
            case TaskStatus::CANCELLED:   statusText = "skip"; break;
            case TaskStatus::SKIPPED:     statusText = "skip"; break;
            default: break;
        }

        lv_obj_t* badge = lv_label_create(card);
        lv_label_set_text(badge, statusText);
        lv_obj_add_style(badge, &style_small, 0);
        lv_obj_set_style_text_color(badge, iconColor, LV_PART_MAIN);
        lv_obj_align(badge, LV_ALIGN_RIGHT_MID, -40, 0);
    }
}

#endif // SCREEN_TASKS_H
