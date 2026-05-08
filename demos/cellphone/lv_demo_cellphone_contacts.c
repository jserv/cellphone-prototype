/**
 * @file lv_demo_cellphone_contacts.c
 */

#include "lv_demo_cellphone_contacts.h"
#include "lv_demo_cellphone_anim.h"
#include "lv_demo_cellphone_data.h"
#include "lv_demo_cellphone_dialer.h"
#include "lv_demo_cellphone_sms.h"
#include "lv_demo_cellphone_softkbd.h"

#if LV_USE_DEMO_CELLPHONE

/*********************
 *      DEFINES
 *********************/
#define CONTACTS_ACTION_H  40
#define CONTACTS_FIELD_H   34

/*********************
 *  STATIC VARIABLES
 *********************/
static int s_selected_idx = -1;

static lv_obj_t * s_contacts_parent;
static lv_obj_t * s_contacts_list;
static lv_obj_t * s_add_overlay;
static lv_obj_t * s_add_form;
static lv_obj_t * s_add_action_row;
static lv_obj_t * s_add_error_label;
static lv_obj_t * s_add_fields[3];

/*********************
 *  STATIC PROTOTYPES
 *********************/
static lv_obj_t * contact_detail_create(lv_obj_t * parent);
static void populate_contacts_list(lv_obj_t * list);
static lv_obj_t * contact_field_create(lv_obj_t * parent,
                                       const char * title,
                                       const char * placeholder,
                                       uint32_t max_len);
static void contact_click_cb(lv_event_t * e);
static void call_btn_cb(lv_event_t * e);
static void msg_btn_cb(lv_event_t * e);
static void add_btn_cb(lv_event_t * e);
static void add_cancel_cb(lv_event_t * e);
static void add_save_cb(lv_event_t * e);
static void add_input_event_cb(lv_event_t * e);
static void add_background_cb(lv_event_t * e);
static void contacts_delete_cb(lv_event_t * e);
static void add_overlay_close(void);
static void add_layout_reflow(bool animated);
static void add_kb_hidden_cb(void);
static bool add_focus_advance(uint32_t from_idx, bool manage_keyboard);

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

lv_obj_t * cellphone_contacts_create(lv_obj_t * parent)
{
    const int32_t header_h = CELLPHONE_SECTION_HDR_H;
    s_contacts_parent = parent;
    s_contacts_list = NULL;
    s_add_overlay = NULL;
    s_add_form = NULL;
    s_add_action_row = NULL;
    s_add_error_label = NULL;
    lv_memzero(s_add_fields, sizeof(s_add_fields));

    lv_obj_t * header = cellphone_section_header(parent, "All Contacts",
                                                 LV_ALIGN_LEFT_MID, "+");
    lv_obj_set_pos(header, 0, 0);

    lv_obj_t * add_btn = lv_button_create(header);
    lv_obj_set_size(add_btn, 36, header_h - 2);
    lv_obj_align(add_btn, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_opa(add_btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(add_btn, 0, 0);
    lv_obj_set_style_shadow_width(add_btn, 0, 0);
    lv_obj_set_style_pad_all(add_btn, 0, 0);
    lv_obj_add_event_cb(add_btn, add_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t * list = lv_list_create(parent);
    s_contacts_list = list;
    lv_obj_set_pos(list, 0, header_h);
    lv_obj_set_size(list, CELLPHONE_CONTENT_W,
                    CELLPHONE_CONTENT_H - header_h);
    lv_obj_set_style_bg_color(list, CELLPHONE_COLOR_BG, 0);
    lv_obj_set_style_border_width(list, 0, 0);
    lv_obj_set_style_radius(list, 0, 0);
    lv_obj_set_style_pad_all(list, 0, 0);
    populate_contacts_list(list);

    lv_obj_add_event_cb(parent, contacts_delete_cb, LV_EVENT_DELETE, NULL);
    return parent;
}

bool cellphone_contacts_test_open_add_overlay(void)
{
    if(!s_contacts_parent) return false;
    add_btn_cb(NULL);
    return s_add_overlay != NULL;
}

lv_obj_t * cellphone_contacts_test_get_add_field(uint32_t idx)
{
    if(idx >= 3) return NULL;
    return s_add_fields[idx];
}

bool cellphone_contacts_test_advance_add_focus(uint32_t from_idx)
{
    return add_focus_advance(from_idx, false);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void populate_contacts_list(lv_obj_t * list)
{
    uint32_t contact_count = cellphone_data_contact_count();
    char prev_initial = '\0';

    lv_obj_clean(list);

    for(uint32_t i = 0; i < contact_count; i++) {
        const cellphone_contact_t * c = cellphone_data_contact_at(i);
        if(c->initial != prev_initial) {
            char hdr[2] = { c->initial, '\0' };
            lv_obj_t * section = lv_list_add_text(list, hdr);
            lv_obj_set_style_text_font(section, CELLPHONE_FONT_SM, 0);
            lv_obj_set_style_text_color(section, CELLPHONE_COLOR_TEXT_SEC, 0);
            cellphone_obj_paint_fill(section, CELLPHONE_COLOR_BG);
            prev_initial = c->initial;
        }

        lv_obj_t * btn = lv_list_add_button(list, NULL, c->name);
        lv_obj_set_style_text_font(btn, CELLPHONE_FONT_NORMAL, 0);
        lv_obj_set_style_text_color(btn, CELLPHONE_COLOR_TEXT, 0);
        cellphone_obj_paint_fill(btn, CELLPHONE_COLOR_CARD);
        lv_obj_set_style_border_width(btn, 0, 0);
        lv_obj_set_style_shadow_width(btn, 0, 0);
        lv_obj_add_event_cb(btn, contact_click_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)i);
    }
}

/* Max input length for the New Contact form, matched to the persistent
 * backing buffers in lv_demo_cellphone_data.c.  Without these, lv_textarea
 * would silently accept characters that lv_snprintf truncates on save. */
#define CONTACT_FIELD_NAME_MAX  31
#define CONTACT_FIELD_PHONE_MAX 23
#define CONTACT_FIELD_EMAIL_MAX 39

static lv_obj_t * contact_field_create(lv_obj_t * parent,
                                       const char * title,
                                       const char * placeholder,
                                       uint32_t max_len)
{
    lv_obj_t * card = cellphone_obj_fill(parent, CELLPHONE_COLOR_CARD);
    lv_obj_set_width(card, lv_pct(100));
    lv_obj_set_height(card, LV_SIZE_CONTENT);
    lv_obj_set_style_radius(card, 10, 0);
    lv_obj_set_style_pad_all(card, 8, 0);
    lv_obj_set_style_pad_row(card, 6, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, CELLPHONE_COLOR_INDICATOR, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    cellphone_label(card, title, CELLPHONE_FONT_SM, CELLPHONE_COLOR_TEXT_SEC);

    lv_obj_t * ta = lv_textarea_create(card);
    lv_obj_set_width(ta, lv_pct(100));
    lv_obj_set_height(ta, CONTACTS_FIELD_H);
    lv_textarea_set_one_line(ta, true);
    lv_textarea_set_max_length(ta, max_len);
    lv_textarea_set_placeholder_text(ta, placeholder);
    lv_obj_set_style_text_font(ta, CELLPHONE_FONT_NORMAL, 0);
    lv_obj_set_style_bg_color(ta, CELLPHONE_COLOR_BG, 0);
    lv_obj_set_style_border_color(ta, CELLPHONE_COLOR_INDICATOR, 0);
    lv_obj_set_style_border_width(ta, 1, 0);
    lv_obj_set_style_radius(ta, 8, 0);
    lv_obj_set_style_pad_left(ta, 10, 0);
    lv_obj_set_style_pad_right(ta, 10, 0);
    lv_obj_set_style_pad_top(ta, 7, 0);
    lv_obj_set_style_pad_bottom(ta, 7, 0);
    lv_obj_set_style_border_color(ta, CELLPHONE_COLOR_PRIMARY, LV_STATE_FOCUSED);
    lv_obj_add_event_cb(ta, add_input_event_cb, LV_EVENT_ALL, NULL);
    return ta;
}

static void add_btn_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    if(!s_contacts_parent || s_add_overlay) return;

    s_add_overlay = cellphone_obj_fill(s_contacts_parent, CELLPHONE_COLOR_BG);
    lv_obj_set_pos(s_add_overlay, 0, 0);
    lv_obj_set_size(s_add_overlay, CELLPHONE_CONTENT_W, CELLPHONE_CONTENT_H);
    lv_obj_set_style_pad_all(s_add_overlay, 0, 0);
    lv_obj_add_event_cb(s_add_overlay, add_background_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_move_foreground(s_add_overlay);

    lv_obj_t * header = cellphone_section_header(s_add_overlay, "New Contact",
                                                 LV_ALIGN_LEFT_MID, NULL);
    lv_obj_set_pos(header, 0, 0);
    lv_obj_add_event_cb(header, add_background_cb, LV_EVENT_CLICKED, NULL);

    s_add_form = cellphone_obj_fill(s_add_overlay, CELLPHONE_COLOR_BG);
    lv_obj_set_pos(s_add_form, 0, CELLPHONE_SECTION_HDR_H);
    lv_obj_set_size(s_add_form, CELLPHONE_CONTENT_W,
                    CELLPHONE_CONTENT_H - CELLPHONE_SECTION_HDR_H - CONTACTS_ACTION_H);
    lv_obj_set_style_pad_all(s_add_form, 10, 0);
    lv_obj_set_style_pad_row(s_add_form, 8, 0);
    lv_obj_set_flex_flow(s_add_form, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scrollbar_mode(s_add_form, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_event_cb(s_add_form, add_background_cb, LV_EVENT_CLICKED, NULL);

    s_add_fields[0] = contact_field_create(s_add_form, "Name", "Ashley Johnson",
                                           CONTACT_FIELD_NAME_MAX);
    s_add_fields[1] = contact_field_create(s_add_form, "Phone", "+1-555-0123",
                                           CONTACT_FIELD_PHONE_MAX);
    s_add_fields[2] = contact_field_create(s_add_form, "Email", "ashley@example.com",
                                           CONTACT_FIELD_EMAIL_MAX);

    s_add_action_row = cellphone_obj_bare(s_add_overlay);
    lv_obj_set_size(s_add_action_row, CELLPHONE_CONTENT_W, CONTACTS_ACTION_H);
    lv_obj_set_pos(s_add_action_row, 0, CELLPHONE_CONTENT_H - CONTACTS_ACTION_H);
    lv_obj_set_style_pad_all(s_add_action_row, 4, 0);
    lv_obj_set_style_pad_column(s_add_action_row, 4, 0);
    lv_obj_set_style_border_color(s_add_action_row, CELLPHONE_COLOR_INDICATOR, 0);
    lv_obj_set_style_border_width(s_add_action_row, 1, 0);
    lv_obj_set_style_border_side(s_add_action_row, LV_BORDER_SIDE_TOP, 0);
    cellphone_obj_paint_fill(s_add_action_row, CELLPHONE_COLOR_NAVBAR);
    lv_obj_set_flex_flow(s_add_action_row, LV_FLEX_FLOW_ROW);

    lv_obj_t * cancel_btn = lv_button_create(s_add_action_row);
    lv_obj_set_size(cancel_btn, (CELLPHONE_CONTENT_W - 12) / 2, CONTACTS_ACTION_H - 8);
    lv_obj_set_style_bg_color(cancel_btn, CELLPHONE_COLOR_CARD, 0);
    lv_obj_set_style_text_color(cancel_btn, CELLPHONE_COLOR_TEXT, 0);
    lv_obj_set_style_radius(cancel_btn, 10, 0);
    lv_obj_add_event_cb(cancel_btn, add_cancel_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_center(cellphone_label(cancel_btn, "Cancel",
                                  CELLPHONE_FONT_NORMAL, CELLPHONE_COLOR_TEXT));

    lv_obj_t * save_btn = lv_button_create(s_add_action_row);
    lv_obj_set_size(save_btn, (CELLPHONE_CONTENT_W - 12) / 2, CONTACTS_ACTION_H - 8);
    lv_obj_set_style_bg_color(save_btn, CELLPHONE_COLOR_PRIMARY, 0);
    lv_obj_set_style_radius(save_btn, 10, 0);
    lv_obj_add_event_cb(save_btn, add_save_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_center(cellphone_label(save_btn, "Save",
                                  CELLPHONE_FONT_NORMAL, CELLPHONE_COLOR_CARD));

    cellphone_softkbd_set_hidden_cb(add_kb_hidden_cb);
}

static void add_cancel_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    add_overlay_close();
}

static void add_save_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    if(!s_contacts_list || !s_add_fields[0]) return;

    const char * name = lv_textarea_get_text(s_add_fields[0]);
    const char * phone = s_add_fields[1] ? lv_textarea_get_text(s_add_fields[1]) : "";
    const char * email = s_add_fields[2] ? lv_textarea_get_text(s_add_fields[2]) : "";
    int32_t idx = cellphone_data_contact_add(name, phone, email);
    if(idx < 0) {
        if(!s_add_action_row) return;
        const char * msg = (name && name[0]) ? "Contact list full" : "Name required";
        if(s_add_error_label && lv_obj_is_valid(s_add_error_label)) {
            lv_label_set_text(s_add_error_label, msg);
        }
        else {
            s_add_error_label = cellphone_label(s_add_action_row, msg,
                                                CELLPHONE_FONT_SM, lv_palette_main(LV_PALETTE_RED));
            lv_obj_align(s_add_error_label, LV_ALIGN_TOP_MID, 0, -16);
        }
        return;
    }

    s_selected_idx = idx;
    populate_contacts_list(s_contacts_list);
    add_overlay_close();
}

static void add_input_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * ta = lv_event_get_target_obj(e);

    /* First tap on an unfocused textarea fires both FOCUSED and CLICKED.
     * Make FOCUSED the canonical open-keyboard path; CLICKED only re-opens
     * after the user dismissed the keyboard via the hide-keyboard key while
     * the textarea kept focus.  Otherwise both events would run the
     * show()+reflow()+scroll() chain twice per tap. */
    if(code == LV_EVENT_CLICKED && cellphone_softkbd_is_visible()) return;
    if(code == LV_EVENT_FOCUSED || code == LV_EVENT_CLICKED) {
        cellphone_softkbd_show(s_contacts_parent, ta);
        add_layout_reflow(true);
        lv_obj_scroll_to_view_recursive(ta, LV_ANIM_ON);
        return;
    }

    if(code == LV_EVENT_READY) {
        for(uint32_t i = 0; i < 3; i++) {
            if(s_add_fields[i] != ta) continue;
            if(!add_focus_advance(i, true)) {
                cellphone_softkbd_hide();
            }
            return;
        }
    }

    if(code == LV_EVENT_CANCEL) {
        cellphone_softkbd_hide();
    }
}

static void add_background_cb(lv_event_t * e)
{
    lv_obj_t * target = lv_event_get_target_obj(e);
    for(uint32_t i = 0; i < 3; i++) {
        if(target == s_add_fields[i]) return;
    }

    cellphone_softkbd_hide();
}

static void contacts_delete_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    cellphone_softkbd_set_hidden_cb(NULL);
    cellphone_softkbd_hide();
    s_contacts_parent = NULL;
    s_contacts_list = NULL;
    s_add_overlay = NULL;
    s_add_form = NULL;
    s_add_action_row = NULL;
    s_add_error_label = NULL;
    lv_memzero(s_add_fields, sizeof(s_add_fields));
}

static void add_overlay_close(void)
{
    cellphone_softkbd_set_hidden_cb(NULL);
    cellphone_softkbd_hide();

    if(s_add_overlay && lv_obj_is_valid(s_add_overlay)) {
        lv_obj_delete(s_add_overlay);
    }

    s_add_overlay = NULL;
    s_add_form = NULL;
    s_add_action_row = NULL;
    s_add_error_label = NULL;
    lv_memzero(s_add_fields, sizeof(s_add_fields));
}

static void add_layout_reflow(bool animated)
{
    if(!s_add_form || !s_add_action_row) return;

    bool kb_visible = cellphone_softkbd_is_visible();
    int32_t kb_h = kb_visible ? cellphone_softkbd_height() : 0;
    int32_t header_h = CELLPHONE_SECTION_HDR_H;
    int32_t action_y = CELLPHONE_CONTENT_H - CONTACTS_ACTION_H - kb_h;
    int32_t form_h = action_y - header_h;
    if(form_h < 72) form_h = 72;

    if(animated) {
        cellphone_anim_run(s_add_action_row, cellphone_anim_set_y_cb,
                           lv_obj_get_y(s_add_action_row), action_y,
                           CELLPHONE_MOTION_QUICK.enter_ms,
                           CELLPHONE_MOTION_QUICK.path_cb);
        cellphone_anim_run(s_add_form, cellphone_anim_set_height_cb,
                           lv_obj_get_height(s_add_form), form_h,
                           CELLPHONE_MOTION_QUICK.enter_ms,
                           CELLPHONE_MOTION_QUICK.path_cb);
    }
    else {
        lv_obj_set_y(s_add_action_row, action_y);
        lv_obj_set_height(s_add_form, form_h);
    }
}

static void add_kb_hidden_cb(void)
{
    add_layout_reflow(false);
}

static bool add_focus_advance(uint32_t from_idx, bool manage_keyboard)
{
    lv_obj_t * current;
    lv_obj_t * next;

    if(from_idx >= 2) return false;

    current = s_add_fields[from_idx];
    next = s_add_fields[from_idx + 1];
    if(!current || !next) return false;

    lv_obj_clear_state(current, LV_STATE_FOCUSED);
    lv_obj_add_state(next, LV_STATE_FOCUSED);

    if(manage_keyboard) {
        cellphone_softkbd_show(s_contacts_parent, next);
        add_layout_reflow(true);
        lv_obj_scroll_to_view_recursive(next, LV_ANIM_ON);
    }

    return true;
}

static void contact_click_cb(lv_event_t * e)
{
    s_selected_idx = (int)(uintptr_t)lv_event_get_user_data(e);
    cellphone_screen_push(contact_detail_create);
}

static lv_obj_t * contact_detail_create(lv_obj_t * parent)
{
    const cellphone_contact_t * c = cellphone_data_contact_at((uint32_t)s_selected_idx);
    if(!c) return parent;

    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(parent, 10, 0);
    lv_obj_set_style_pad_row(parent, 6, 0);
    lv_obj_set_style_bg_color(parent, CELLPHONE_COLOR_BG, 0);

    lv_obj_t * avatar = lv_obj_create(parent);
    lv_obj_set_size(avatar, 50, 50);
    lv_obj_set_style_radius(avatar, LV_RADIUS_CIRCLE, 0);
    cellphone_obj_paint_fill(avatar, CELLPHONE_COLOR_PRIMARY);
    lv_obj_set_style_border_width(avatar, 0, 0);
    lv_obj_set_scrollbar_mode(avatar, LV_SCROLLBAR_MODE_OFF);

    char initial_str[2] = { c->initial, '\0' };
    lv_obj_t * initial_label = cellphone_label(avatar, initial_str,
                                               CELLPHONE_FONT_HEADING, CELLPHONE_COLOR_CARD);
    lv_obj_center(initial_label);

    cellphone_label(parent, c->name, CELLPHONE_FONT_HEADING, CELLPHONE_COLOR_TEXT);
    cellphone_label(parent, c->phone, CELLPHONE_FONT_NORMAL, CELLPHONE_COLOR_TEXT_SEC);
    cellphone_label(parent, c->email, CELLPHONE_FONT_SM, CELLPHONE_COLOR_TEXT_SEC);

    lv_obj_t * spacer = lv_obj_create(parent);
    lv_obj_set_size(spacer, 1, 1);
    lv_obj_set_flex_grow(spacer, 1);
    lv_obj_set_style_bg_opa(spacer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(spacer, 0, 0);
    lv_obj_set_scrollbar_mode(spacer, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t * btn_row = lv_obj_create(parent);
    lv_obj_set_size(btn_row, LV_PCT(90), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(btn_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_row, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(btn_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_row, 0, 0);
    lv_obj_set_style_pad_all(btn_row, 4, 0);
    lv_obj_set_scrollbar_mode(btn_row, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t * call_btn = lv_button_create(btn_row);
    lv_obj_set_size(call_btn, CELLPHONE_BTN_MIN * 2, CELLPHONE_BTN_MIN);
    lv_obj_set_style_bg_color(call_btn, CELLPHONE_COLOR_CALL_GREEN, 0);
    lv_obj_set_style_radius(call_btn, 8, 0);
    lv_obj_add_event_cb(call_btn, call_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_center(cellphone_label(call_btn, "Call",
                                  CELLPHONE_FONT_NORMAL, CELLPHONE_COLOR_CARD));

    lv_obj_t * msg_btn = lv_button_create(btn_row);
    lv_obj_set_size(msg_btn, CELLPHONE_BTN_MIN * 2, CELLPHONE_BTN_MIN);
    lv_obj_set_style_bg_color(msg_btn, CELLPHONE_COLOR_PRIMARY, 0);
    lv_obj_set_style_radius(msg_btn, 8, 0);
    lv_obj_add_event_cb(msg_btn, msg_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_center(cellphone_label(msg_btn, "Message",
                                  CELLPHONE_FONT_NORMAL, CELLPHONE_COLOR_CARD));

    return parent;
}

static void call_btn_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    const cellphone_contact_t * c = cellphone_data_contact_at((uint32_t)s_selected_idx);
    if(!c) return;
    cellphone_dialer_call_contact(c->name, c->phone);
}

static void msg_btn_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    const cellphone_contact_t * c = cellphone_data_contact_at((uint32_t)s_selected_idx);
    if(!c) return;
    cellphone_sms_open_chat_with(c->name);
}

#endif
