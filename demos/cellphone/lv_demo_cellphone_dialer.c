/**
 * @file lv_demo_cellphone_dialer.c
 *
 * Phone dialer: numeric keypad, number display, and in-call screen.
 */

#include "lv_demo_cellphone_dialer.h"

#if LV_USE_DEMO_CELLPHONE

/*********************
 *      DEFINES
 *********************/
#define DIALER_NUM_MAX      20
#define DIALER_KEYPAD_ROWS  4
#define DIALER_KEYPAD_COLS  3

/**********************
 *  STATIC PROTOTYPES
 **********************/
static lv_obj_t * incall_create(lv_obj_t * parent);
static void keypad_cb(lv_event_t * e);
static void delete_cb(lv_event_t * e);
static void call_cb(lv_event_t * e);
static void end_call_cb(lv_event_t * e);
static void incall_delete_cb(lv_event_t * e);
static void timer_cb(lv_timer_t * t);
static void update_number_display(void);
static lv_obj_t * action_button(lv_obj_t * parent, int32_t w, int32_t h,
                                lv_color_t bg, lv_color_t fg,
                                const lv_font_t * font, const char * text,
                                lv_event_cb_t cb);
static lv_obj_t * dialer_key_create(lv_obj_t * parent, const char * text, int32_t col, int32_t row);

/**********************
 *  STATIC VARIABLES
 **********************/
static char        s_number[DIALER_NUM_MAX + 1];
static uint32_t    s_number_len;
static lv_obj_t  * s_number_label;

/* in-call state.
 *
 * The in-call screen has two entry points: the keypad (which dials the
 * digits accumulated in s_number) and contacts (which dials a known
 * contact's phone number, with the contact's name shown above it). The
 * pair below decouples the in-call display from s_number so the keypad
 * path keeps working while contact-initiated calls show the name. */
static const char * s_call_name;        /* NULL when dialing a raw number */
static const char * s_call_number;      /* points into compiled-in data or s_number */
static uint32_t    s_call_seconds;
static lv_obj_t  * s_time_label;
static lv_timer_t * s_call_timer;

static const char * const s_keypad_labels[DIALER_KEYPAD_ROWS][DIALER_KEYPAD_COLS] = {
    { "1", "2", "3" },
    { "4", "5", "6" },
    { "7", "8", "9" },
    { "*", "0", "#" },
};

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

lv_obj_t * cellphone_dialer_create(lv_obj_t * parent)
{
    /* reset dialed number on each creation */
    s_number_len = 0;
    s_number[0] = '\0';

    int32_t w = CELLPHONE_CONTENT_W;

    /* --- number display --- */
    s_number_label = cellphone_label(parent, "", CELLPHONE_FONT_LARGE, CELLPHONE_COLOR_TEXT);
    lv_obj_set_width(s_number_label, w - 16);
    lv_label_set_long_mode(s_number_label, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_align(s_number_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_pad_top(s_number_label, 6, 0);
    lv_obj_set_style_pad_right(s_number_label, 8, 0);

    /* --- layout geometry ---
     * Use flex column on parent so the keypad grows to fill all remaining
     * space.  This avoids hardcoding CELLPHONE_CONTENT_H, which can differ
     * from the actual container height when a skin is active. */
    int32_t number_h = 36;
    int32_t bottom_h = CELLPHONE_BTN_MIN;
    int32_t gap = 4;

    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(parent, gap, 0);
    lv_obj_set_style_pad_all(parent, 0, 0);

    /* number display: fixed height */
    lv_obj_set_height(s_number_label, number_h);

    /* --- 3x4 keypad --- */
    static const int32_t col_dsc[] = {
        LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST
    };
    static const int32_t row_dsc[] = {
        LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST
    };

    lv_obj_t * keypad = cellphone_obj_bare(parent);
    lv_obj_set_width(keypad, w);
    lv_obj_set_flex_grow(keypad, 1);  /* fill remaining vertical space */
    lv_obj_set_layout(keypad, LV_LAYOUT_GRID);
    lv_obj_set_grid_dsc_array(keypad, col_dsc, row_dsc);
    lv_obj_set_style_pad_all(keypad, 3, 0);
    lv_obj_set_style_pad_row(keypad, 3, 0);
    lv_obj_set_style_pad_column(keypad, 3, 0);
    lv_obj_clear_flag(keypad, LV_OBJ_FLAG_SCROLLABLE);

    for(int32_t row = 0; row < DIALER_KEYPAD_ROWS; row++) {
        for(int32_t col = 0; col < DIALER_KEYPAD_COLS; col++) {
            dialer_key_create(keypad, s_keypad_labels[row][col], col, row);
        }
    }

    /* --- bottom row: delete + call --- */
    lv_obj_t * bottom = cellphone_obj_bare(parent);
    lv_obj_set_size(bottom, w, bottom_h);
    lv_obj_set_flex_flow(bottom, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bottom, LV_FLEX_ALIGN_SPACE_EVENLY,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(bottom, LV_OBJ_FLAG_SCROLLABLE);

    action_button(bottom, w / 3, bottom_h - 4,
                  CELLPHONE_COLOR_CARD, CELLPHONE_COLOR_TEXT,
                  CELLPHONE_FONT_HEADING, LV_SYMBOL_BACKSPACE, delete_cb);

    action_button(bottom, w * 2 / 3 - 8, bottom_h - 4,
                  CELLPHONE_COLOR_CALL_GREEN, lv_color_white(),
                  CELLPHONE_FONT_HEADING, LV_SYMBOL_CALL " Call", call_cb);

    return parent;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static lv_obj_t * action_button(lv_obj_t * parent, int32_t w, int32_t h,
                                lv_color_t bg, lv_color_t fg,
                                const lv_font_t * font, const char * text,
                                lv_event_cb_t cb)
{
    lv_obj_t * btn = lv_button_create(parent);
    lv_obj_set_size(btn, w, h);
    cellphone_obj_paint_fill(btn, bg);
    lv_obj_set_style_radius(btn, 8, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);

    lv_obj_center(cellphone_label(btn, text, font, fg));

    return btn;
}

static lv_obj_t * dialer_key_create(lv_obj_t * parent, const char * text, int32_t col, int32_t row)
{
    lv_obj_t * btn = lv_button_create(parent);
    lv_obj_remove_style_all(btn);
    lv_obj_set_grid_cell(btn, LV_GRID_ALIGN_STRETCH, col, 1,
                         LV_GRID_ALIGN_STRETCH, row, 1);
    lv_obj_set_style_bg_color(btn, CELLPHONE_COLOR_CARD, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(btn, 8, 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_add_event_cb(btn, keypad_cb, LV_EVENT_CLICKED, (void *)text);

    lv_obj_t * lbl = cellphone_label(btn, text, CELLPHONE_FONT_LARGE, CELLPHONE_COLOR_TEXT);
    lv_obj_center(lbl);

    return btn;
}

static void update_number_display(void)
{
    if(s_number_label) {
        lv_label_set_text_static(s_number_label, s_number);
    }
}

static void keypad_cb(lv_event_t * e)
{
    const char * txt = lv_event_get_user_data(e);
    if(txt == NULL) return;

    if(s_number_len < DIALER_NUM_MAX) {
        s_number[s_number_len] = txt[0];
        s_number_len++;
        s_number[s_number_len] = '\0';
        update_number_display();
    }
}

static void delete_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    if(s_number_len > 0) {
        s_number_len--;
        s_number[s_number_len] = '\0';
        update_number_display();
    }
}

static void call_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    if(s_number_len == 0) return;
    /* Keypad path: no contact name, dial the digits in s_number. */
    s_call_name = NULL;
    s_call_number = s_number;
    cellphone_screen_push(incall_create);
}

void cellphone_dialer_call_contact(const char * name, const char * phone)
{
    if(!phone || phone[0] == '\0') return;
    /* The strings come from the const data tables (flash on MCU); they
     * outlive any screen the user can navigate to from contacts. */
    s_call_name = (name && name[0]) ? name : NULL;
    s_call_number = phone;
    cellphone_screen_push(incall_create);
}

/* --- in-call screen --- */

static void timer_cb(lv_timer_t * t)
{
    LV_UNUSED(t);
    if(s_time_label == NULL) return;
    s_call_seconds++;
    uint32_t mins = s_call_seconds / 60;
    uint32_t secs = s_call_seconds % 60;
    lv_label_set_text_fmt(s_time_label, "%02" LV_PRIu32 ":%02" LV_PRIu32,
                          mins, secs);
}

static void incall_delete_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    s_time_label = NULL;
    if(s_call_timer) {
        lv_timer_delete(s_call_timer);
        s_call_timer = NULL;
    }
}

static void end_call_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    cellphone_screen_pop();
}

static lv_obj_t * incall_create(lv_obj_t * parent)
{
    s_call_seconds = 0;

    int32_t w = CELLPHONE_CONTENT_W;
    const char * num_text = s_call_number ? s_call_number : "";

    /* "Calling..." label */
    lv_obj_t * lbl_status = cellphone_label(parent, "Calling...",
                                            CELLPHONE_FONT_NORMAL, CELLPHONE_COLOR_TEXT_SEC);
    lv_obj_align(lbl_status, LV_ALIGN_TOP_MID, 0, 16);

    /* When a contact name is known, render it big and the number small
     * underneath -- the user wants to see who they're calling, not a string
     * of digits. Keypad path keeps the digits-only layout it always had. */
    int32_t time_y;
    if(s_call_name) {
        lv_obj_t * lbl_name = cellphone_label(parent, s_call_name,
                                              CELLPHONE_FONT_LARGE, CELLPHONE_COLOR_TEXT);
        lv_obj_align(lbl_name, LV_ALIGN_TOP_MID, 0, 40);

        lv_obj_t * lbl_num = cellphone_label(parent, num_text,
                                             CELLPHONE_FONT_NORMAL, CELLPHONE_COLOR_TEXT_SEC);
        lv_obj_align(lbl_num, LV_ALIGN_TOP_MID, 0, 68);
        time_y = 96;
    }
    else {
        lv_obj_t * lbl_num = cellphone_label(parent, num_text,
                                             CELLPHONE_FONT_LARGE, CELLPHONE_COLOR_TEXT);
        lv_obj_align(lbl_num, LV_ALIGN_TOP_MID, 0, 40);
        time_y = 72;
    }

    /* elapsed time */
    s_time_label = cellphone_label(parent, "00:00",
                                   CELLPHONE_FONT_HEADING, CELLPHONE_COLOR_TEXT_SEC);
    lv_obj_align(s_time_label, LV_ALIGN_TOP_MID, 0, time_y);

    /* 1-second timer for elapsed time */
    s_call_timer = lv_timer_create(timer_cb, 1000, NULL);

    lv_obj_t * btn_end = action_button(parent, w / 2, 36,
                                       CELLPHONE_COLOR_CALL_RED, lv_color_white(),
                                       CELLPHONE_FONT_NORMAL,
                                       LV_SYMBOL_CALL " End Call", end_call_cb);
    lv_obj_align(btn_end, LV_ALIGN_BOTTOM_MID, 0, -12);
    lv_obj_set_style_radius(btn_end, 18, 0);

    /* clean up timer when screen is destroyed */
    lv_obj_add_event_cb(parent, incall_delete_cb, LV_EVENT_DELETE, NULL);

    return parent;
}

#endif /* LV_USE_DEMO_CELLPHONE */
