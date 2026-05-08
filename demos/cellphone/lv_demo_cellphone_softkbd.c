/**
 * @file lv_demo_cellphone_softkbd.c
 *
 * The keyboard owns no global state beyond a single instance pointer; if the
 * host screen is destroyed (e.g. user pops the chat), the keyboard goes with
 * it and the next call to cellphone_softkbd_show() rebuilds it.
 */

#include "lv_demo_cellphone_softkbd.h"
#include "lv_demo_cellphone_anim.h"

#if LV_USE_DEMO_CELLPHONE

#if LV_USE_KEYBOARD

/*********************
 *      DEFINES
 *********************/
#define SOFTKBD_ANIM_MS 180

/*********************
 *  STATIC VARIABLES
 *********************/
static lv_obj_t * s_kb;       /* live keyboard, NULL when hidden/destroyed */
static lv_obj_t * s_ta;       /* attached textarea, tracked for delete safety */
static bool       s_visible;  /* tracks animation target state */
static cellphone_softkbd_hidden_cb_t s_hidden_cb;

/*********************
 *  STATIC PROTOTYPES
 *********************/
static lv_obj_t * softkbd_build(lv_obj_t * parent);
static void softkbd_apply_layout(lv_obj_t * kb);
static void softkbd_apply_style(lv_obj_t * kb);
static void softkbd_attach_ta(lv_obj_t * ta);
static void softkbd_detach_ta(void);
static void softkbd_ta_delete_cb(lv_event_t * e);
static void softkbd_kb_value_changed_cb(lv_event_t * e);
static void softkbd_kb_event_cb(lv_event_t * e);
static void softkbd_kb_delete_cb(lv_event_t * e);
static void softkbd_hide_finish_cb(lv_anim_t * a);

/*********************
 *  KEYBOARD LAYOUTS
 *********************/

/* Cellphone-friendly layout — 10/9/9/4 instead of LVGL's default 12/11/12/5.
 *
 * Glyph notes for the bundled icon fallback subset:
 *   Renderable here: LV_SYMBOL_CLOSE (F00D), LV_SYMBOL_BACKSPACE
 *   (F55A), LV_SYMBOL_RIGHT (F054), and the media/status icons used
 *   elsewhere in the demo.
 *   LV_SYMBOL_OK (F00C), LV_SYMBOL_KEYBOARD (F11C) and
 *   LV_SYMBOL_NEW_LINE (F8A2) still do not render — never use them here.
 *
 * lv_keyboard's default value-changed handler recognizes:
 *   "abc"/"ABC"/"1#" — mode switches
 *   LV_SYMBOL_BACKSPACE — delete one char
 *   LV_SYMBOL_OK — fire LV_EVENT_READY (send) — but no glyph!
 *   LV_SYMBOL_CLOSE / LV_SYMBOL_KEYBOARD — fire LV_EVENT_CANCEL (hide)
 *
 * For the send key we use LV_SYMBOL_RIGHT (right arrow → renders, reads
 * as "send") and route it directly to the textarea in
 * softkbd_kb_value_changed_cb.  Unlike stock READY handling, send does
 * not dismiss the keyboard; that matches modern chat composers where
 * consecutive messages are common.
 */

/* Mode-switch buttons need the CHECKED bit so lv_keyboard's update_ctrl_map
 * can paint the active mode.  Other function keys (backspace, send, hide)
 * must NOT carry CHECKED — otherwise the LV_STATE_CHECKED style we use to
 * highlight the active mode-switch leaks onto every function key. */
#define KB_MODE  (LV_BUTTONMATRIX_CTRL_NO_REPEAT | LV_BUTTONMATRIX_CTRL_CLICK_TRIG | LV_BUTTONMATRIX_CTRL_CHECKED)
#define KB_FN    (LV_BUTTONMATRIX_CTRL_NO_REPEAT | LV_BUTTONMATRIX_CTRL_CLICK_TRIG)

static const char * const kb_map_lower[] = {
    "q", "w", "e", "r", "t", "y", "u", "i", "o", "p", "\n",
    "a", "s", "d", "f", "g", "h", "j", "k", "l", "\n",
    "ABC", "z", "x", "c", "v", "b", "n", "m", LV_SYMBOL_BACKSPACE, "\n",
    "1#", " ", LV_SYMBOL_RIGHT, LV_SYMBOL_CLOSE, ""
};

static const char * const kb_map_upper[] = {
    "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "\n",
    "A", "S", "D", "F", "G", "H", "J", "K", "L", "\n",
    "abc", "Z", "X", "C", "V", "B", "N", "M", LV_SYMBOL_BACKSPACE, "\n",
    "1#", " ", LV_SYMBOL_RIGHT, LV_SYMBOL_CLOSE, ""
};

/* Special map: drop the redundant "abc" that used to sit on row 3.  Row 3
 * leftmost is now an apostrophe, freeing the slot for a high-frequency SMS
 * char.  Row 2 swaps "&*" / "%" for "_" "(" ")" — closer to messaging
 * usage. */
static const char * const kb_map_special[] = {
    "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "\n",
    "@", "#", "$", "_", "(", ")", "-", "+", "=", "\n",
    "'", "!", "?", ",", ".", ":", ";", "/", LV_SYMBOL_BACKSPACE, "\n",
    "abc", " ", LV_SYMBOL_RIGHT, LV_SYMBOL_CLOSE, ""
};

/* Width units per row: 10 / 9 / 11 / 10.  Letters get ~22-27 px each on
 * QVGA, mode-switch / backspace / send / close all 48 px, space bar 96 px.
 * Function keys (backspace, send, close) use KB_FN — no CHECKED — so they
 * don't inherit the active-mode highlight that lives on KB_MODE keys. */
static const lv_buttonmatrix_ctrl_t kb_ctrl_letters[] = {
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1,
    KB_MODE | 2, 1, 1, 1, 1, 1, 1, 1, KB_FN | 2,
    KB_MODE | 2, 4, KB_FN | 2, KB_FN | 2
};

static const lv_buttonmatrix_ctrl_t kb_ctrl_special[] = {
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1,
    /* Apostrophe leftmost is plain (no flags) so it inserts a char rather
     * than acting like a mode switch. */
    2, 1, 1, 1, 1, 1, 1, 1, KB_FN | 2,
    KB_MODE | 2, 4, KB_FN | 2, KB_FN | 2
};

/*********************
 *   GLOBAL FUNCTIONS
 *********************/

void cellphone_softkbd_show(lv_obj_t * host_parent, lv_obj_t * ta)
{
    if(!host_parent || !ta) return;

    /* Reparent if the existing keyboard belongs to a different host. */
    if(s_kb && lv_obj_get_parent(s_kb) != host_parent) {
        softkbd_detach_ta();
        lv_obj_delete(s_kb);
        s_kb = NULL;
    }

    if(!s_kb) {
        s_kb = softkbd_build(host_parent);
    }

    softkbd_attach_ta(ta);
    lv_keyboard_set_textarea(s_kb, ta);
    lv_obj_remove_flag(s_kb, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_kb);

    /* Cancel any in-flight slide-out before re-driving the slide-in. */
    lv_anim_delete(s_kb, cellphone_anim_set_y_cb);

    int32_t parent_h = lv_obj_get_height(host_parent);
    int32_t target_y = parent_h - CELLPHONE_SOFTKBD_H;
    int32_t start_y  = s_visible ? lv_obj_get_y(s_kb) : parent_h;

    lv_obj_set_y(s_kb, start_y);

    cellphone_anim_run(s_kb, cellphone_anim_set_y_cb,
                       start_y, target_y, SOFTKBD_ANIM_MS,
                       lv_anim_path_ease_out);

    s_visible = true;
}

void cellphone_softkbd_hide(void)
{
    if(!s_kb || !s_visible) return;

    s_visible = false;

    int32_t parent_h = lv_obj_get_height(lv_obj_get_parent(s_kb));
    int32_t start_y  = lv_obj_get_y(s_kb);

    lv_anim_delete(s_kb, cellphone_anim_set_y_cb);

    /* Built inline because the hide path also needs a completed_cb. */
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_kb);
    lv_anim_set_exec_cb(&a, cellphone_anim_set_y_cb);
    lv_anim_set_values(&a, start_y, parent_h);
    lv_anim_set_duration(&a, SOFTKBD_ANIM_MS);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in);
    lv_anim_set_completed_cb(&a, softkbd_hide_finish_cb);
    lv_anim_start(&a);
}

bool cellphone_softkbd_is_visible(void)
{
    return s_kb != NULL && s_visible;
}

int32_t cellphone_softkbd_height(void)
{
    return CELLPHONE_SOFTKBD_H;
}

void cellphone_softkbd_set_hidden_cb(cellphone_softkbd_hidden_cb_t cb)
{
    s_hidden_cb = cb;
}

/*********************
 *  STATIC FUNCTIONS
 *********************/

static lv_obj_t * softkbd_build(lv_obj_t * parent)
{
    lv_obj_t * kb = lv_keyboard_create(parent);
    /* lv_keyboard_constructor anchors itself with LV_ALIGN_BOTTOM_MID, which
     * overrides any lv_obj_set_y() we do during the slide animation on the
     * next layout pass.  Restore default (top-left) alignment so explicit
     * y positions stick. */
    lv_obj_set_align(kb, LV_ALIGN_DEFAULT);
    lv_obj_set_size(kb, lv_obj_get_width(parent), CELLPHONE_SOFTKBD_H);
    lv_obj_set_pos(kb, 0, lv_obj_get_height(parent));
    /* Popovers help on QVGA where keys are 22-27 px wide and the user's
     * thumb fully occludes the key it's pressing. */
    lv_keyboard_set_popovers(kb, true);

    softkbd_apply_layout(kb);
    softkbd_apply_style(kb);

    /* Replace lv_keyboard's default value-changed handler with our wrapper
     * — this is how we translate the right-arrow "send" key into a
     * textarea-only READY event.  The wrapper falls through to
     * lv_keyboard_def_event_cb for everything else so mode switches,
     * backspace, and CLOSE keep working. */
    lv_obj_remove_event_cb(kb, lv_keyboard_def_event_cb);
    lv_obj_add_event_cb(kb, softkbd_kb_value_changed_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(kb, softkbd_kb_event_cb,         LV_EVENT_READY,         NULL);
    lv_obj_add_event_cb(kb, softkbd_kb_event_cb,         LV_EVENT_CANCEL,        NULL);
    lv_obj_add_event_cb(kb, softkbd_kb_delete_cb,        LV_EVENT_DELETE,        NULL);

    return kb;
}

static void softkbd_apply_layout(lv_obj_t * kb)
{
    lv_keyboard_set_map(kb, LV_KEYBOARD_MODE_TEXT_LOWER, kb_map_lower,   kb_ctrl_letters);
    lv_keyboard_set_map(kb, LV_KEYBOARD_MODE_TEXT_UPPER, kb_map_upper,   kb_ctrl_letters);
    lv_keyboard_set_map(kb, LV_KEYBOARD_MODE_SPECIAL,    kb_map_special, kb_ctrl_special);
    lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_TEXT_LOWER);
}

static void softkbd_apply_style(lv_obj_t * kb)
{
    /* Use the normal font (14 px) on key labels — at 22-27 px key width the
     * extra two pixels over font_sm matter for legibility.  Tight padding so
     * the keys themselves get every available pixel. */
    lv_obj_set_style_text_font(kb, CELLPHONE_FONT_NORMAL, 0);
    lv_obj_set_style_pad_top(kb, 4, 0);
    lv_obj_set_style_pad_bottom(kb, 4, 0);
    lv_obj_set_style_pad_left(kb, 3, 0);
    lv_obj_set_style_pad_right(kb, 3, 0);
    lv_obj_set_style_pad_gap(kb, 3, 0);
    lv_obj_set_style_radius(kb, 0, 0);
    lv_obj_set_style_border_width(kb, 0, 0);
    cellphone_obj_paint_fill(kb, CELLPHONE_COLOR_NAVBAR);
    /* 1-px top border so the keyboard reads as a distinct band when sliding
     * over a busy chat history. */
    lv_obj_set_style_border_color(kb, CELLPHONE_COLOR_INDICATOR, 0);
    lv_obj_set_style_border_width(kb, 1, 0);
    lv_obj_set_style_border_side(kb, LV_BORDER_SIDE_TOP, 0);

    /* Style each key (LV_PART_ITEMS) — flat, slightly rounded, themed. */
    lv_obj_set_style_text_font(kb, CELLPHONE_FONT_NORMAL, LV_PART_ITEMS);
    lv_obj_set_style_text_color(kb, CELLPHONE_COLOR_TEXT, LV_PART_ITEMS);
    lv_obj_set_style_bg_color(kb, CELLPHONE_COLOR_CARD, LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(kb, LV_OPA_COVER, LV_PART_ITEMS);
    lv_obj_set_style_radius(kb, 6, LV_PART_ITEMS);
    lv_obj_set_style_border_width(kb, 0, LV_PART_ITEMS);
    lv_obj_set_style_shadow_width(kb, 0, LV_PART_ITEMS);
    lv_obj_set_style_pad_top(kb, 6, LV_PART_ITEMS);
    lv_obj_set_style_pad_bottom(kb, 6, LV_PART_ITEMS);

    /* Pressed feedback: stronger than just a fill-color swap so users see
     * the touch register even when their thumb covers the key. */
    lv_obj_set_style_bg_color(kb, CELLPHONE_COLOR_PRIMARY, LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_set_style_text_color(kb, lv_color_white(), LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_set_style_translate_y(kb, 1, LV_PART_ITEMS | LV_STATE_PRESSED);

    /* Mode-switch keys (CHECKED state) get the dark primary so the active
     * mode is visually distinct from the (transient) pressed state. */
    lv_obj_set_style_bg_color(kb, CELLPHONE_COLOR_PRIMARY_DK, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_text_color(kb, lv_color_white(), LV_PART_ITEMS | LV_STATE_CHECKED);
}

static void softkbd_attach_ta(lv_obj_t * ta)
{
    if(s_ta == ta) return;
    softkbd_detach_ta();
    s_ta = ta;
    lv_obj_add_event_cb(ta, softkbd_ta_delete_cb, LV_EVENT_DELETE, NULL);
}

static void softkbd_detach_ta(void)
{
    if(!s_ta) return;
    lv_obj_remove_event_cb(s_ta, softkbd_ta_delete_cb);
    s_ta = NULL;
}

static void softkbd_ta_delete_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    /* Attached textarea is being deleted — clear the keyboard's reference
     * before the pointer goes invalid, then dismiss the keyboard. */
    s_ta = NULL;
    if(s_kb) lv_keyboard_set_textarea(s_kb, NULL);
    cellphone_softkbd_hide();
}

static void softkbd_kb_value_changed_cb(lv_event_t * e)
{
    lv_obj_t * kb = lv_event_get_target_obj(e);
    uint32_t btn = lv_keyboard_get_selected_button(kb);
    const char * txt = lv_keyboard_get_button_text(kb, btn);

    /* Custom send key: LV_SYMBOL_RIGHT sends through the textarea only and
     * keeps the keyboard open so the user can continue typing. */
    if(txt && lv_strcmp(txt, LV_SYMBOL_RIGHT) == 0) {
        lv_obj_t * ta = lv_keyboard_get_textarea(kb);
        if(ta) lv_obj_send_event(ta, LV_EVENT_READY, NULL);
        return;
    }

    /* All other keys (letters, mode switches, backspace, close) get the
     * stock behavior. */
    lv_keyboard_def_event_cb(e);
}

static void softkbd_kb_event_cb(lv_event_t * e)
{
    /* Only explicit hide actions dismiss the keyboard. */
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_CANCEL) {
        cellphone_softkbd_hide();
    }
}

static void softkbd_kb_delete_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    /* Host screen is being torn down — detach the textarea cb (if the ta
     * is also a descendant of the same host, it's about to be freed) and
     * drop the dangling pointers so the next show() builds a fresh kb. */
    softkbd_detach_ta();
    s_kb = NULL;
    s_visible = false;
}

static void softkbd_hide_finish_cb(lv_anim_t * a)
{
    LV_UNUSED(a);
    /* If show() was called mid-animation, s_visible is already true and
     * we must not stomp on the new state. */
    if(!s_kb || s_visible) return;
    lv_obj_add_flag(s_kb, LV_OBJ_FLAG_HIDDEN);
    lv_keyboard_set_textarea(s_kb, NULL);
    /* Hand off to the host so it can reflow only after the keyboard is
     * fully off-screen, avoiding the chat-jumps-under-still-visible-kb
     * glitch. */
    if(s_hidden_cb) s_hidden_cb();
}

#else

void cellphone_softkbd_show(lv_obj_t * host_parent, lv_obj_t * ta)
{
    LV_UNUSED(host_parent);
    LV_UNUSED(ta);
}

void cellphone_softkbd_hide(void)
{
}

bool cellphone_softkbd_is_visible(void)
{
    return false;
}

int32_t cellphone_softkbd_height(void)
{
    return CELLPHONE_SOFTKBD_H;
}

void cellphone_softkbd_set_hidden_cb(cellphone_softkbd_hidden_cb_t cb)
{
    LV_UNUSED(cb);
}

#endif /* LV_USE_KEYBOARD */
#endif /* LV_USE_DEMO_CELLPHONE */
