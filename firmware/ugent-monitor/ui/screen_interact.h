/**
 * UGENT ESP32 Monitor — Interaction Screen
 *
 * Human-in-the-loop: approve/reject actions, send text responses.
 */

#ifndef SCREEN_INTERACT_H
#define SCREEN_INTERACT_H

#include <lvgl.h>
#include "ui_theme.h"
#include "../config.h"
#include "../ugent_client.h"

static lv_obj_t* interact_question = nullptr;
static lv_obj_t* interact_approve_btn = nullptr;
static lv_obj_t* interact_reject_btn = nullptr;
static lv_obj_t* interact_input = nullptr;
static lv_obj_t* interact_send_btn = nullptr;
static lv_obj_t* interact_history = nullptr;
static lv_obj_t* interact_status = nullptr;

// Ring buffer for message history
static char msg_history[MAX_MESSAGES_HISTORY][MAX_MESSAGE_LEN];
static uint8_t msg_history_count = 0;
static UgentClient* interact_client_ = nullptr;

inline void interact_add_message(const char* msg) {
    if (msg_history_count < MAX_MESSAGES_HISTORY) {
        strncpy(msg_history[msg_history_count], msg, MAX_MESSAGE_LEN - 1);
        msg_history[msg_history_count][MAX_MESSAGE_LEN - 1] = '\0';
        msg_history_count++;
    } else {
        // Shift left
        for (uint8_t i = 0; i < MAX_MESSAGES_HISTORY - 1; i++) {
            memcpy(msg_history[i], msg_history[i + 1], MAX_MESSAGE_LEN);
        }
        strncpy(msg_history[MAX_MESSAGES_HISTORY - 1], msg, MAX_MESSAGE_LEN - 1);
        msg_history[MAX_MESSAGES_HISTORY - 1][MAX_MESSAGE_LEN - 1] = '\0';
    }

    // Update history display
    if (interact_history) {
        lv_obj_clean(interact_history);
        for (uint8_t i = 0; i < msg_history_count; i++) {
            lv_obj_t* lbl = lv_label_create(interact_history);
            lv_label_set_text(lbl, msg_history[i]);
            lv_obj_add_style(lbl, &style_small, 0);
            lv_obj_set_style_text_color(lbl, color_subtext(), LV_PART_MAIN);
            lv_obj_set_width(lbl, SCREEN_WIDTH - 40);
        }
    }
}

// Button callbacks
static void approve_btn_cb(lv_event_t* e) {
    if (interact_client_) {
        interact_client_->approveAction();
        lv_label_set_text(interact_status, "Approved!");
        lv_obj_set_style_text_color(interact_status, color_green(), LV_PART_MAIN);
        interact_add_message("[SENT] Approved");
    }
}

static void reject_btn_cb(lv_event_t* e) {
    if (interact_client_) {
        interact_client_->rejectAction();
        lv_label_set_text(interact_status, "Rejected!");
        lv_obj_set_style_text_color(interact_status, color_red(), LV_PART_MAIN);
        interact_add_message("[SENT] Rejected");
    }
}

static void send_btn_cb(lv_event_t* e) {
    if (interact_client_ && interact_input) {
        const char* text = lv_textarea_get_text(interact_input);
        if (strlen(text) > 0) {
            interact_client_->sendInteractionResponse(text);
            interact_add_message(text);
            lv_textarea_set_text(interact_input, "");
            lv_label_set_text(interact_status, "Sent!");
            lv_obj_set_style_text_color(interact_status, color_green(), LV_PART_MAIN);
        }
    }
}

inline void screen_interact_create(lv_obj_t* parent) {
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
    lv_label_set_text(header, "Interaction");
    lv_obj_add_style(header, &style_title, 0);
    lv_obj_align(header, LV_ALIGN_TOP_LEFT, 4, 0);

    // Status feedback
    interact_status = lv_label_create(cont);
    lv_label_set_text(interact_status, "");
    lv_obj_add_style(interact_status, &style_small, 0);
    lv_obj_align(interact_status, LV_ALIGN_TOP_RIGHT, -4, 4);

    // Question/interaction panel
    lv_obj_t* q_panel = lv_obj_create(cont);
    lv_obj_set_size(q_panel, SCREEN_WIDTH - 20, 50);
    lv_obj_align(q_panel, LV_ALIGN_TOP_LEFT, 4, 22);
    lv_obj_set_style_bg_color(q_panel, color_surface0(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(q_panel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(q_panel, 6, LV_PART_MAIN);
    lv_obj_set_style_border_color(q_panel, color_mauve(), LV_PART_MAIN);
    lv_obj_set_style_border_width(q_panel, 1, LV_PART_MAIN);
    lv_obj_set_style_pad_all(q_panel, 6, LV_PART_MAIN);

    interact_question = lv_label_create(q_panel);
    lv_label_set_text(interact_question, "Waiting for interaction...");
    lv_obj_add_style(interact_question, &style_body, 0);
    lv_obj_set_width(interact_question, SCREEN_WIDTH - 40);
    lv_obj_align(interact_question, LV_ALIGN_TOP_LEFT, 0, 0);

    // Approve/Reject buttons
    interact_approve_btn = lv_btn_create(cont);
    lv_obj_set_size(interact_approve_btn, 140, 28);
    lv_obj_align_to(interact_approve_btn, q_panel, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 6);
    lv_obj_add_style(interact_approve_btn, &style_btn_primary, 0);
    lv_obj_add_event_cb(interact_approve_btn, approve_btn_cb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* approve_label = lv_label_create(interact_approve_btn);
    lv_label_set_text(approve_label, "Approve");
    lv_obj_center(approve_label);

    interact_reject_btn = lv_btn_create(cont);
    lv_obj_set_size(interact_reject_btn, 140, 28);
    lv_obj_align_to(interact_reject_btn, q_panel, LV_ALIGN_OUT_BOTTOM_RIGHT, 0, 6);
    lv_obj_add_style(interact_reject_btn, &style_btn_danger, 0);
    lv_obj_add_event_cb(interact_reject_btn, reject_btn_cb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* reject_label = lv_label_create(interact_reject_btn);
    lv_label_set_text(reject_label, "Reject");
    lv_obj_center(reject_label);

    // Text input + send
    interact_input = lv_textarea_create(cont);
    lv_obj_set_size(interact_input, SCREEN_WIDTH - 70, 28);
    lv_obj_align_to(interact_input, interact_approve_btn, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 6);
    lv_textarea_set_placeholder_text(interact_input, "Type response...");
    lv_textarea_set_one_line(interact_input, true);
    lv_obj_set_style_bg_color(interact_input, color_surface0(), LV_PART_MAIN);
    lv_obj_set_style_text_color(interact_input, color_text(), LV_PART_MAIN);
    lv_obj_set_style_border_color(interact_input, color_surface1(), LV_PART_MAIN);

    interact_send_btn = lv_btn_create(cont);
    lv_obj_set_size(interact_send_btn, 50, 28);
    lv_obj_align_to(interact_send_btn, interact_input, LV_ALIGN_OUT_RIGHT_MID, 4, 0);
    lv_obj_add_style(interact_send_btn, &style_btn_primary, 0);
    lv_obj_add_event_cb(interact_send_btn, send_btn_cb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* send_label = lv_label_create(interact_send_btn);
    lv_label_set_text(send_label, "Send");
    lv_obj_center(send_label);

    // Message history
    lv_obj_t* hist_header = lv_label_create(cont);
    lv_label_set_text(hist_header, "History");
    lv_obj_add_style(hist_header, &style_small, 0);
    lv_obj_set_style_text_color(hist_header, color_overlay0(), LV_PART_MAIN);
    lv_obj_align_to(hist_header, interact_input, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 6);

    interact_history = lv_obj_create(cont);
    lv_obj_set_size(interact_history, SCREEN_WIDTH - 20, SCREEN_HEIGHT - 218);
    lv_obj_align_to(interact_history, hist_header, LV_ALIGN_OUT_BOTTOM_LEFT, -4, 2);
    lv_obj_set_style_bg_color(interact_history, color_surface0(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(interact_history, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(interact_history, 4, LV_PART_MAIN);
    lv_obj_set_style_border_width(interact_history, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(interact_history, 4, LV_PART_MAIN);
    lv_obj_set_flex_flow(interact_history, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_gap(interact_history, 2);
    lv_obj_add_flag(interact_history, LV_OBJ_FLAG_SCROLLABLE);
}

// Set the UGENT client for button callbacks
inline void screen_interact_set_client(UgentClient* client) {
    interact_client_ = client;
}

// Update interaction from SSE event
inline void screen_interact_show_question(const char* question) {
    if (interact_question) {
        lv_label_set_text(interact_question, question);
    }
}

#endif // SCREEN_INTERACT_H
