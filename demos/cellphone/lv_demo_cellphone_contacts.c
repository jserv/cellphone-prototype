/**
 * @file lv_demo_cellphone_contacts.c
 */

#include "lv_demo_cellphone_contacts.h"
#include "lv_demo_cellphone_data.h"
#include "lv_demo_cellphone_dialer.h"
#include "lv_demo_cellphone_sms.h"

#if LV_USE_DEMO_CELLPHONE

/*********************
 *  STATIC VARIABLES
 *********************/
static int s_selected_idx;

/*********************
 *  STATIC PROTOTYPES
 *********************/
static lv_obj_t * contact_detail_create(lv_obj_t * parent);
static void contact_click_cb(lv_event_t * e);
static void call_btn_cb(lv_event_t * e);
static void msg_btn_cb(lv_event_t * e);

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

lv_obj_t * cellphone_contacts_create(lv_obj_t * parent)
{
    const cellphone_contact_t * contacts = cellphone_data_contacts();

    /* Reference UI: a thin header strip above the list shows the section
     * title ("All Contacts") on the left and a "+" affordance on the right.
     * Use the navbar gradient palette so the strip reads as chrome, not a
     * list row. Height tracks CELLPHONE_SECTION_HDR_H so large mode (28 px)
     * matches the section dividers below. */
    const int32_t header_h = CELLPHONE_SECTION_HDR_H;

    lv_obj_t * header = cellphone_obj_bar(parent, CELLPHONE_COLOR_NAVBAR_GRAD,
                                          CELLPHONE_COLOR_NAVBAR);
    lv_obj_set_pos(header, 0, 0);
    lv_obj_set_size(header, CELLPHONE_CONTENT_W, header_h);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * title = cellphone_label(header, "All Contacts",
                                       CELLPHONE_FONT_SM, CELLPHONE_COLOR_TEXT);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 8, 0);

    /* The "+" is a visual affordance only -- the demo has no add-contact
     * flow. Rendering it as a plain label (no LV_OBJ_FLAG_CLICKABLE, no
     * pressed feedback) keeps the UI honest: nothing depresses, nothing
     * lies about being interactive. ASCII "+" reads cleaner than
     * LV_SYMBOL_PLUS at this small header height. */
    lv_obj_t * add = cellphone_label(header, "+",
                                     CELLPHONE_FONT_HEADING, CELLPHONE_COLOR_TEXT);
    lv_obj_align(add, LV_ALIGN_RIGHT_MID, -8, 0);

    lv_obj_t * list = lv_list_create(parent);
    lv_obj_set_pos(list, 0, header_h);
    lv_obj_set_size(list, CELLPHONE_CONTENT_W,
                    CELLPHONE_CONTENT_H - header_h);
    lv_obj_set_style_bg_color(list, CELLPHONE_COLOR_BG, 0);
    lv_obj_set_style_border_width(list, 0, 0);
    lv_obj_set_style_radius(list, 0, 0);
    lv_obj_set_style_pad_all(list, 0, 0);

    char prev_initial = '\0';
    int i;

    for(i = 0; i < CELLPHONE_CONTACT_COUNT; i++) {
        /* insert a section header when the initial letter changes */
        if(contacts[i].initial != prev_initial) {
            char hdr[2] = { contacts[i].initial, '\0' };
            lv_obj_t * section = lv_list_add_text(list, hdr);
            lv_obj_set_style_text_font(section, CELLPHONE_FONT_SM, 0);
            lv_obj_set_style_text_color(section, CELLPHONE_COLOR_TEXT_SEC, 0);
            cellphone_obj_paint_fill(section, CELLPHONE_COLOR_BG);
            prev_initial = contacts[i].initial;
        }

        lv_obj_t * btn = lv_list_add_button(list, NULL, contacts[i].name);
        lv_obj_set_style_text_font(btn, CELLPHONE_FONT_NORMAL, 0);
        lv_obj_set_style_text_color(btn, CELLPHONE_COLOR_TEXT, 0);
        /* Default theme paints list buttons pure white, which clashes with
         * the olive list bg and the cream surfaces used elsewhere. Force
         * each row to CELLPHONE_COLOR_CARD so the contacts screen reads as
         * cream rows separated by olive section bands -- one coherent
         * two-tone palette instead of three competing tones. */
        cellphone_obj_paint_fill(btn, CELLPHONE_COLOR_CARD);
        lv_obj_set_style_border_width(btn, 0, 0);
        lv_obj_set_style_shadow_width(btn, 0, 0);
        /* Carry the row index in the event descriptor instead of per-object
         * user_data so list buttons do not need spec_attr allocations just
         * to launch the detail screen. */
        lv_obj_add_event_cb(btn, contact_click_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    }

    return parent;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/
static void contact_click_cb(lv_event_t * e)
{
    s_selected_idx = (int)(intptr_t)lv_event_get_user_data(e);
    cellphone_screen_push(contact_detail_create);
}

static lv_obj_t * contact_detail_create(lv_obj_t * parent)
{
    if(s_selected_idx < 0 || s_selected_idx >= CELLPHONE_CONTACT_COUNT) return parent;

    const cellphone_contact_t * contacts = cellphone_data_contacts();
    const cellphone_contact_t * c = &contacts[s_selected_idx];

    /* flex column, centered */
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(parent, 10, 0);
    lv_obj_set_style_pad_row(parent, 6, 0);
    lv_obj_set_style_bg_color(parent, CELLPHONE_COLOR_BG, 0);

    /* avatar: colored circle with the initial letter */
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

    /* name */
    cellphone_label(parent, c->name, CELLPHONE_FONT_HEADING, CELLPHONE_COLOR_TEXT);

    /* phone */
    cellphone_label(parent, c->phone, CELLPHONE_FONT_NORMAL, CELLPHONE_COLOR_TEXT_SEC);

    /* email */
    cellphone_label(parent, c->email, CELLPHONE_FONT_SM, CELLPHONE_COLOR_TEXT_SEC);

    /* spacer to push buttons toward the bottom */
    lv_obj_t * spacer = lv_obj_create(parent);
    lv_obj_set_size(spacer, 1, 1);
    lv_obj_set_flex_grow(spacer, 1);
    lv_obj_set_style_bg_opa(spacer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(spacer, 0, 0);
    lv_obj_set_scrollbar_mode(spacer, LV_SCROLLBAR_MODE_OFF);

    /* action button row */
    lv_obj_t * btn_row = lv_obj_create(parent);
    lv_obj_set_size(btn_row, LV_PCT(90), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(btn_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_row, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(btn_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_row, 0, 0);
    lv_obj_set_style_pad_all(btn_row, 4, 0);
    lv_obj_set_scrollbar_mode(btn_row, LV_SCROLLBAR_MODE_OFF);

    /* "Call" button -- launches dialer */
    lv_obj_t * call_btn = lv_button_create(btn_row);
    lv_obj_set_size(call_btn, CELLPHONE_BTN_MIN * 2, CELLPHONE_BTN_MIN);
    lv_obj_set_style_bg_color(call_btn, CELLPHONE_COLOR_CALL_GREEN, 0);
    lv_obj_set_style_radius(call_btn, 8, 0);
    lv_obj_add_event_cb(call_btn, call_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_center(cellphone_label(call_btn, "Call",
                                  CELLPHONE_FONT_NORMAL, CELLPHONE_COLOR_CARD));

    /* "Message" button -- launches SMS */
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
    if(s_selected_idx < 0 || s_selected_idx >= CELLPHONE_CONTACT_COUNT) return;
    const cellphone_contact_t * c = &cellphone_data_contacts()[s_selected_idx];
    cellphone_dialer_call_contact(c->name, c->phone);
}

static void msg_btn_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    if(s_selected_idx < 0 || s_selected_idx >= CELLPHONE_CONTACT_COUNT) return;
    const cellphone_contact_t * c = &cellphone_data_contacts()[s_selected_idx];
    cellphone_sms_open_chat_with(c->name);
}

#endif
