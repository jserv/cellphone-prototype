/**
 * @file lv_demo_cellphone_sms.c
 *
 * SMS messaging app: conversation list + chat detail view.
 */

#include "lv_demo_cellphone_sms.h"
#include "lv_demo_cellphone_anim.h"
#include "lv_demo_cellphone_data.h"
#include "lv_demo_cellphone_softkbd.h"

#if LV_USE_DEMO_CELLPHONE

/*********************
 *      DEFINES
 *********************/
#define SMS_BUBBLE_RADIUS   12
#define SMS_BUBBLE_PAD      8
#define SMS_BUBBLE_MAX_PCT  70
#define SMS_INPUT_H         34

/*********************
 *  STATIC VARIABLES
 *********************/
/* Chat detail reads its data through s_current_thread, never the index.
 * That lets the thread-list path point at a real entry while the
 * contacts-app path can synthesize an empty thread for a contact who
 * has never exchanged messages -- one render code path, no special
 * cases inside chat_detail_create. */
static const cellphone_sms_thread_t * s_current_thread;
static cellphone_sms_thread_t s_synthetic_thread; /* used when contact has no history */

static lv_obj_t * s_chat_parent;     /* the screen content area, host for the keyboard */
static lv_obj_t * s_chat_container;
static lv_obj_t * s_chat_input;
static lv_obj_t * s_chat_input_row;
static lv_obj_t * s_chat_send_btn;

/*********************
 *  STATIC PROTOTYPES
 *********************/
static lv_obj_t * chat_detail_create(lv_obj_t * parent);
static void thread_click_cb(lv_event_t * e);
static void send_btn_cb(lv_event_t * e);
static void chat_delete_cb(lv_event_t * e);
static void chat_input_event_cb(lv_event_t * e);
static void chat_background_event_cb(lv_event_t * e);
static void chat_kb_hidden_cb(void);
static void append_bubble(lv_obj_t * cont, const char * text,
                          const char * time, bool is_sent);
static void chat_apply_kb_layout(bool kb_visible);
static void chat_send_message(void);
static void chat_refresh_compose_state(void);
static void chat_layout_reflow(bool animated);
static void chat_reflow_done_cb(lv_anim_t * a);

/*********************
 *   GLOBAL FUNCTIONS
 *********************/

lv_obj_t * cellphone_sms_create(lv_obj_t * parent)
{
    const cellphone_sms_thread_t * threads = cellphone_data_sms_threads();

    /* scrollable list filling the parent content area */
    lv_obj_t * list = lv_list_create(parent);
    lv_obj_set_size(list, CELLPHONE_CONTENT_W, CELLPHONE_CONTENT_H);
    lv_obj_set_style_bg_color(list, CELLPHONE_COLOR_BG, 0);
    lv_obj_set_style_border_width(list, 0, 0);
    lv_obj_set_style_radius(list, 0, 0);
    lv_obj_set_style_pad_all(list, 0, 0);
    lv_obj_align(list, LV_ALIGN_TOP_LEFT, 0, 0);

    uint32_t i;
    for(i = 0; i < CELLPHONE_SMS_THREAD_COUNT; i++) {
        /* build display text: "Name\nPreview..." */
        lv_obj_t * btn = lv_list_add_button(list, NULL, threads[i].contact_name);
        cellphone_obj_paint_fill(btn, CELLPHONE_COLOR_CARD);
        lv_obj_set_style_pad_ver(btn, 6, 0);
        lv_obj_set_height(btn, LV_SIZE_CONTENT);

        /* the list button already has a label child; restyle it as the contact name */
        lv_obj_t * name_label = lv_obj_get_child(btn, 0);
        lv_obj_set_style_text_font(name_label, CELLPHONE_FONT_NORMAL, 0);
        lv_obj_set_style_text_color(name_label, CELLPHONE_COLOR_TEXT, 0);

        /* preview label below the name */
        lv_obj_t * preview = cellphone_label(btn, threads[i].preview,
                                             CELLPHONE_FONT_SM, CELLPHONE_COLOR_TEXT_SEC);
        lv_label_set_long_mode(preview, LV_LABEL_LONG_DOT);
        lv_obj_set_width(preview, CELLPHONE_CONTENT_W - 24);
        lv_obj_set_style_text_letter_space(preview, 1, 0);

        /* timestamp on the right side of the name */
        lv_obj_t * ts = cellphone_label(btn, threads[i].timestamp,
                                        CELLPHONE_FONT_SM, CELLPHONE_COLOR_TEXT_SEC);
        lv_obj_align(ts, LV_ALIGN_TOP_RIGHT, -4, 0);

        /* store thread index and register click handler */
        lv_obj_add_event_cb(btn, thread_click_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)i);
    }

    return parent;
}

/*********************
 *  STATIC FUNCTIONS
 *********************/

static void thread_click_cb(lv_event_t * e)
{
    uint32_t idx = (uint32_t)(uintptr_t)lv_event_get_user_data(e);
    if(idx >= CELLPHONE_SMS_THREAD_COUNT) return;
    s_current_thread = &cellphone_data_sms_threads()[idx];
    cellphone_screen_push(chat_detail_create);
}

void cellphone_sms_open_chat_with(const char * name)
{
    if(!name || name[0] == '\0') return;

    /* Reuse an existing thread when the contact already has chat
     * history; otherwise present an empty chat keyed only by the
     * contact's name.  This is what users expect from a contacts
     * app -- the message button always lands them in a usable chat. */
    const cellphone_sms_thread_t * threads = cellphone_data_sms_threads();
    s_current_thread = NULL;
    uint32_t i;
    for(i = 0; i < CELLPHONE_SMS_THREAD_COUNT; i++) {
        if(lv_strcmp(threads[i].contact_name, name) == 0) {
            s_current_thread = &threads[i];
            break;
        }
    }
    if(!s_current_thread) {
        s_synthetic_thread.contact_name = name;
        s_synthetic_thread.preview      = "";
        s_synthetic_thread.timestamp    = "";
        s_synthetic_thread.messages     = NULL;
        s_synthetic_thread.msg_count    = 0;
        s_current_thread = &s_synthetic_thread;
    }
    cellphone_screen_push(chat_detail_create);
}

static lv_obj_t * chat_detail_create(lv_obj_t * parent)
{
    if(!s_current_thread) return parent;
    const cellphone_sms_thread_t * thread = s_current_thread;
    s_chat_parent = parent;

    /* Header strip names the conversation -- without it, an empty chat
     * (e.g. messaging a contact for the first time) gives the user no
     * cue about who they're talking to. Mirrors the contacts-list
     * header so chrome reads consistently across the app. The size
     * macro is also used by chat_apply_kb_layout(); using it directly
     * here keeps both paths anchored to a single source of truth. */
    lv_obj_t * header = cellphone_section_header(parent, thread->contact_name,
                                                 LV_ALIGN_CENTER, NULL);
    lv_obj_set_pos(header, 0, 0);

    int32_t container_h = CELLPHONE_CONTENT_H - SMS_INPUT_H - CELLPHONE_SECTION_HDR_H;

    /* scrollable message container (saved for send_btn_cb) */
    lv_obj_t * cont = cellphone_obj_fill(parent, CELLPHONE_COLOR_BG);
    s_chat_container = cont;
    lv_obj_set_size(cont, CELLPHONE_CONTENT_W, container_h);
    lv_obj_set_pos(cont, 0, CELLPHONE_SECTION_HDR_H);
    lv_obj_set_style_pad_all(cont, 4, 0);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_scrollbar_mode(cont, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_add_event_cb(cont, chat_background_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(header, chat_background_event_cb, LV_EVENT_CLICKED, NULL);

    /* create message bubbles */
    uint32_t i;
    for(i = 0; i < thread->msg_count; i++) {
        const cellphone_sms_msg_t * msg = &thread->messages[i];
        append_bubble(cont, msg->text, msg->time, msg->is_sent != 0);
    }

    /* scroll to the latest message */
    lv_obj_scroll_to_y(cont, LV_COORD_MAX, LV_ANIM_OFF);

    /* Null out stale pointers when screen is destroyed */
    lv_obj_add_event_cb(parent, chat_delete_cb, LV_EVENT_DELETE, NULL);

    /* input row at bottom: textarea + send button.  Position is set
     * explicitly (not via lv_obj_align) so chat_apply_kb_layout() can
     * slide the row up while the keyboard is visible. */
    lv_obj_t * input_row = cellphone_obj_bare(parent);
    s_chat_input_row = input_row;
    lv_obj_set_size(input_row, CELLPHONE_CONTENT_W, SMS_INPUT_H);
    lv_obj_set_pos(input_row, 0, CELLPHONE_CONTENT_H - SMS_INPUT_H);
    lv_obj_set_flex_flow(input_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(input_row, 2, 0);
    cellphone_obj_paint_fill(input_row, CELLPHONE_COLOR_NAVBAR);
    lv_obj_set_style_pad_all(input_row, 2, 0);
    lv_obj_set_style_border_color(input_row, CELLPHONE_COLOR_INDICATOR, 0);
    lv_obj_set_style_border_width(input_row, 1, 0);
    lv_obj_set_style_border_side(input_row, LV_BORDER_SIDE_TOP, 0);

    lv_obj_t * input = lv_textarea_create(input_row);
    lv_obj_set_height(input, SMS_INPUT_H - 4);
    lv_obj_set_flex_grow(input, 1);
    lv_textarea_set_placeholder_text(input, "Type a message...");
    lv_textarea_set_one_line(input, true);
    /* Match the keyboard's key font so typed text reads at the same weight
     * as the key that produced it. */
    lv_obj_set_style_text_font(input, CELLPHONE_FONT_NORMAL, 0);
    lv_obj_set_style_bg_color(input, CELLPHONE_COLOR_CARD, 0);
    lv_obj_set_style_border_color(input, CELLPHONE_COLOR_INDICATOR, 0);
    lv_obj_set_style_border_width(input, 1, 0);
    lv_obj_set_style_radius(input, 10, 0);
    lv_obj_set_style_pad_left(input, 10, 0);
    lv_obj_set_style_pad_right(input, 10, 0);
    lv_obj_set_style_pad_top(input, 7, 0);
    lv_obj_set_style_pad_bottom(input, 7, 0);
    lv_obj_set_style_border_color(input, CELLPHONE_COLOR_PRIMARY, LV_STATE_FOCUSED);

    /* One callback is enough for the textarea lifecycle: raise the keyboard
     * on focus/tap and treat READY as send. This avoids stacking multiple
     * event descriptors on a widget that exists on every chat screen. */
    lv_obj_add_event_cb(input, chat_input_event_cb, LV_EVENT_ALL, NULL);

    /* Reflow the chat layout only after the keyboard finishes sliding out,
     * so the chat container doesn't jump back under a still-visible kb. */
    cellphone_softkbd_set_hidden_cb(chat_kb_hidden_cb);

    lv_obj_t * send_btn = lv_button_create(input_row);
    lv_obj_set_size(send_btn, 44, SMS_INPUT_H - 4);
    lv_obj_set_style_bg_color(send_btn, CELLPHONE_COLOR_PRIMARY, 0);
    lv_obj_set_style_bg_color(send_btn, CELLPHONE_COLOR_PRIMARY_DK, LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(send_btn, LV_OPA_40, LV_STATE_DISABLED);
    lv_obj_set_style_text_opa(send_btn, LV_OPA_50, LV_STATE_DISABLED);
    lv_obj_set_style_radius(send_btn, 10, 0);
    s_chat_input = input;
    s_chat_send_btn = send_btn;
    lv_obj_add_event_cb(send_btn, send_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_center(cellphone_label(send_btn, LV_SYMBOL_RIGHT,
                                  CELLPHONE_FONT_NORMAL, lv_color_white()));
    chat_refresh_compose_state();

    return parent;
}

/**
 * Send button: take text from the textarea, append a "sent" bubble to the
 * message container, then clear the input.
 */
static void send_btn_cb(lv_event_t * e)
{
    if(lv_obj_has_state(lv_event_get_target_obj(e), LV_STATE_DISABLED)) return;
    chat_send_message();
}

static void chat_send_message(void)
{
    if(!s_chat_input || !s_chat_container) return;

    const char * text = lv_textarea_get_text(s_chat_input);
    if(!text || text[0] == '\0') return;

    append_bubble(s_chat_container, text, "now", true);
    lv_textarea_set_text(s_chat_input, "");
    lv_obj_scroll_to_y(s_chat_container, LV_COORD_MAX, LV_ANIM_ON);
    chat_refresh_compose_state();
}

/**
 * Pop the keyboard up when the user taps the input field.
 */
static void chat_input_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if(code == LV_EVENT_CLICKED || code == LV_EVENT_FOCUSED) {
        if(!s_chat_parent || !s_chat_input) return;
        if(cellphone_softkbd_is_visible()) return;

        cellphone_softkbd_show(s_chat_parent, s_chat_input);
        chat_layout_reflow(true);
        return;
    }

    if(code == LV_EVENT_VALUE_CHANGED) {
        chat_refresh_compose_state();
        return;
    }

    if(code == LV_EVENT_READY) {
        chat_send_message();
        chat_refresh_compose_state();
    }
    /* No chat_apply_kb_layout(false) here — chat_kb_hidden_cb handles it
     * after the keyboard's slide-out completes. */
}

static void chat_background_event_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    if(!cellphone_softkbd_is_visible()) return;
    cellphone_softkbd_hide();
}

static void chat_kb_hidden_cb(void)
{
    chat_layout_reflow(true);
}

/**
 * Reflow chat container + input row to make room for the soft keyboard.
 * When @p kb_visible is false, restore the original full-height layout.
 */
static void chat_apply_kb_layout(bool kb_visible)
{
    if(!s_chat_container || !s_chat_input_row) return;

    int32_t kb_h = kb_visible ? cellphone_softkbd_height() : 0;
    /* Container starts below the header strip, so subtract its height
     * here too -- otherwise the bubbles would scroll behind the header. */
    int32_t cont_h = (CELLPHONE_CONTENT_H - SMS_INPUT_H - CELLPHONE_SECTION_HDR_H) - kb_h;
    if(cont_h < 0) cont_h = 0;

    lv_obj_set_height(s_chat_container, cont_h);
    lv_obj_set_y(s_chat_input_row, CELLPHONE_CONTENT_H - SMS_INPUT_H - kb_h);
    /* Keep the latest message in view as the container resizes. */
    lv_obj_scroll_to_y(s_chat_container, LV_COORD_MAX, LV_ANIM_OFF);
}

static void chat_reflow_done_cb(lv_anim_t * a)
{
    /* Snap the scroll to the bottom only after the container has settled
     * at its final height. Calling lv_obj_scroll_to_y mid-animation would
     * compute the target offset against the still-shrinking geometry,
     * leaving a visible gap below the latest bubble. */
    lv_obj_t * cont = (lv_obj_t *)lv_anim_get_user_data(a);
    if(cont) lv_obj_scroll_to_y(cont, LV_COORD_MAX, LV_ANIM_OFF);
}

static void chat_layout_reflow(bool animated)
{
    bool kb_visible = cellphone_softkbd_is_visible();
    int32_t kb_h = kb_visible ? cellphone_softkbd_height() : 0;
    int32_t cont_h = (CELLPHONE_CONTENT_H - SMS_INPUT_H - CELLPHONE_SECTION_HDR_H) - kb_h;
    int32_t row_y = CELLPHONE_CONTENT_H - SMS_INPUT_H - kb_h;

    if(cont_h < 0) cont_h = 0;

    if(!animated) {
        chat_apply_kb_layout(kb_visible);
        return;
    }

    if(!s_chat_container || !s_chat_input_row) return;

    /* Drive the container height inline so we can hang a completed_cb on it
     * to snap the scroll once the geometry is stable. */
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_chat_container);
    lv_anim_set_exec_cb(&a, cellphone_anim_set_height_cb);
    lv_anim_set_values(&a, lv_obj_get_height(s_chat_container), cont_h);
    lv_anim_set_duration(&a, CELLPHONE_MOTION_QUICK.enter_ms);
    lv_anim_set_path_cb(&a, CELLPHONE_MOTION_QUICK.path_cb);
    lv_anim_set_user_data(&a, s_chat_container);
    lv_anim_set_completed_cb(&a, chat_reflow_done_cb);
    lv_anim_start(&a);

    cellphone_anim_run(s_chat_input_row, cellphone_anim_set_y_cb,
                       lv_obj_get_y(s_chat_input_row), row_y,
                       CELLPHONE_MOTION_QUICK.enter_ms,
                       CELLPHONE_MOTION_QUICK.path_cb);
}

static void chat_refresh_compose_state(void)
{
    if(!s_chat_input || !s_chat_send_btn) return;

    const char * text = lv_textarea_get_text(s_chat_input);
    bool has_text = text != NULL && text[0] != '\0';

    /* Toggle CLICKABLE in lockstep with DISABLED so the pressed-state
     * fill never paints on a button that won't act on the press. */
    if(has_text) {
        lv_obj_remove_state(s_chat_send_btn, LV_STATE_DISABLED);
        lv_obj_add_flag(s_chat_send_btn, LV_OBJ_FLAG_CLICKABLE);
    }
    else {
        lv_obj_add_state(s_chat_send_btn, LV_STATE_DISABLED);
        lv_obj_remove_flag(s_chat_send_btn, LV_OBJ_FLAG_CLICKABLE);
    }
}

static void append_bubble(lv_obj_t * cont, const char * text,
                          const char * time, bool is_sent)
{
    int32_t bubble_max_w = (CELLPHONE_CONTENT_W * SMS_BUBBLE_MAX_PCT) / 100;

    /* wrapper row to control alignment */
    lv_obj_t * row = cellphone_obj_bare(cont);
    lv_obj_set_size(row, CELLPHONE_CONTENT_W - 8, LV_SIZE_CONTENT);
    /* Bubbles are display-only; clearing CLICKABLE lets background taps
     * reach chat_background_event_cb on the cont so the kb dismisses. */
    lv_obj_remove_flag(row, LV_OBJ_FLAG_CLICKABLE);

    /* bubble */
    lv_obj_t * bubble = cellphone_obj_fill(row,
                                           is_sent ? CELLPHONE_COLOR_SMS_SENT
                                           : CELLPHONE_COLOR_SMS_RECV);
    lv_obj_set_width(bubble, LV_SIZE_CONTENT);
    lv_obj_set_style_max_width(bubble, bubble_max_w, 0);
    lv_obj_set_height(bubble, LV_SIZE_CONTENT);
    lv_obj_set_style_radius(bubble, SMS_BUBBLE_RADIUS, 0);
    lv_obj_set_style_pad_all(bubble, SMS_BUBBLE_PAD, 0);
    lv_obj_set_scrollbar_mode(bubble, LV_SCROLLBAR_MODE_OFF);
    lv_obj_align(bubble, is_sent ? LV_ALIGN_TOP_RIGHT : LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_remove_flag(bubble, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t * text_label = cellphone_label(bubble, text,
                                            CELLPHONE_FONT_NORMAL, CELLPHONE_COLOR_TEXT);
    lv_label_set_long_mode(text_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(text_label, LV_SIZE_CONTENT);
    lv_obj_set_style_max_width(text_label, bubble_max_w - 2 * SMS_BUBBLE_PAD, 0);

    lv_obj_t * time_label = cellphone_label(row, time,
                                            CELLPHONE_FONT_SM, CELLPHONE_COLOR_TEXT_SEC);
    lv_obj_align_to(time_label, bubble,
                    is_sent ? LV_ALIGN_OUT_BOTTOM_RIGHT : LV_ALIGN_OUT_BOTTOM_LEFT, 0, 2);
}

static void chat_delete_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    /* The keyboard is a child of the chat parent and goes with it; the
     * softkbd module's own delete cb resets its internal state.  Clear
     * the hidden_cb registration so a deferred animation completion
     * can't reach back into the freed chat. */
    cellphone_softkbd_set_hidden_cb(NULL);
    s_chat_container = NULL;
    s_chat_input = NULL;
    s_chat_input_row = NULL;
    s_chat_send_btn = NULL;
    s_chat_parent = NULL;
}

#endif
