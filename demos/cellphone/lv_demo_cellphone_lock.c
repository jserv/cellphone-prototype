/**
 * @file lv_demo_cellphone_lock.c
 *
 * Lock screen with clock, date, and slide-to-unlock gesture.
 */

#include "lv_demo_cellphone_lock.h"
#include "lv_demo_cellphone_anim.h"

#if LV_USE_DEMO_CELLPHONE

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void slider_event_cb(lv_event_t * e);
static void slider_reset_anim_cb(void * var, int32_t value);
static void hint_text_opa_cb(void * var, int32_t value);
static void lock_clock_timer_cb(lv_timer_t * timer);
static void lock_delete_cb(lv_event_t * e);
static void shutter_done_cb(lv_anim_t * a);

static lv_obj_t * s_lock_clock;
static lv_obj_t * s_lock_date;
static lv_timer_t * s_lock_timer;
static bool s_shutter_started;

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

lv_obj_t * cellphone_lock_create(lv_obj_t * parent)
{
    /* Hide status bar and nav bar -- lock screen is full-screen */
    cellphone_chrome_set_visible(false);
    s_shutter_started = false;

    /* Expand parent to cover the entire screen area (override content inset).
     * Add extra height to ensure full coverage regardless of font metrics. */
    lv_obj_set_pos(parent, 0, -CELLPHONE_CONTENT_Y);
    lv_obj_set_size(parent, CELLPHONE_HOR_RES, CELLPHONE_VER_RES + CELLPHONE_CONTENT_Y);

    /* Dark gradient background -- hardcoded so the lock screen remains a
     * high-contrast backdrop for the white clock/date text regardless of
     * the active theme palette. */
    cellphone_obj_paint_grad(parent, lv_color_hex(0x1a1a2e), lv_color_black());

    /* Clock label -- real system time */
    char time_buf[8];
    cellphone_time_now(time_buf, sizeof(time_buf));
    s_lock_clock = cellphone_label(parent, time_buf, CELLPHONE_FONT_CLOCK, lv_color_white());
    lv_obj_align(s_lock_clock, LV_ALIGN_TOP_MID, 0, 60);

    /* Date label -- real system date */
    char date_buf[40];
    cellphone_date_now(date_buf, sizeof(date_buf));
    s_lock_date = cellphone_label(parent, date_buf, CELLPHONE_FONT_NORMAL, lv_color_white());
    lv_obj_align_to(s_lock_date, s_lock_clock, LV_ALIGN_OUT_BOTTOM_MID, 0, 8);

    /* Timer to keep clock live (every 10 seconds for responsiveness) */
    s_lock_timer = lv_timer_create(lock_clock_timer_cb, 10000, NULL);

    /* Clean up timer when lock screen is destroyed */
    lv_obj_add_event_cb(parent, lock_delete_cb, LV_EVENT_DELETE, NULL);

    lv_obj_t * hint_label = cellphone_label(parent, "Slide to unlock",
                                            CELLPHONE_FONT_SM, lv_color_white());
    lv_obj_align(hint_label, LV_ALIGN_BOTTOM_MID, 0, -50);
    lv_obj_set_style_text_opa(hint_label, LV_OPA_80, 0);

    /* Breathing affordance so the hint reads as "interactive" instead of
     * static chrome. Built inline because cellphone_anim_run does not
     * expose playback/repeat; the anim is bound to the label and is
     * implicitly cancelled when the lock screen tears down its parent.
     * Drive text_opa directly so the visible glyph alpha matches the
     * intended pulse range instead of multiplying against a base text_opa. */
    lv_anim_t hint_pulse;
    lv_anim_init(&hint_pulse);
    lv_anim_set_var(&hint_pulse, hint_label);
    lv_anim_set_exec_cb(&hint_pulse, hint_text_opa_cb);
    lv_anim_set_values(&hint_pulse, LV_OPA_30, LV_OPA_80);
    lv_anim_set_duration(&hint_pulse, 900);
    lv_anim_set_playback_duration(&hint_pulse, 900);
    lv_anim_set_repeat_count(&hint_pulse, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&hint_pulse, lv_anim_path_ease_in_out);
    lv_anim_start(&hint_pulse);

    /* Slide-to-unlock slider */
    lv_obj_t * slider = lv_slider_create(parent);
    lv_slider_set_range(slider, 0, 100);
    lv_slider_set_value(slider, 0, LV_ANIM_OFF);
    lv_obj_set_width(slider, CELLPHONE_HOR_RES - 60);
    lv_obj_align(slider, LV_ALIGN_BOTTOM_MID, 0, -20);

    /* Knob styled as a circle with arrow */
    lv_obj_set_style_pad_all(slider, 4, LV_PART_KNOB);
    lv_obj_set_style_radius(slider, LV_RADIUS_CIRCLE, LV_PART_KNOB);
    lv_obj_set_style_bg_color(slider, lv_color_white(), LV_PART_KNOB);

    /* Indicator and main bar styling */
    lv_obj_set_style_bg_color(slider, lv_color_hex(0x444444), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(slider, LV_OPA_80, LV_PART_MAIN);
    lv_obj_set_style_bg_color(slider, CELLPHONE_COLOR_PRIMARY, LV_PART_INDICATOR);

    /* Arrow symbol on the knob area -- use a label overlaid on slider */
    lv_obj_t * arrow = cellphone_label(slider, LV_SYMBOL_RIGHT,
                                       CELLPHONE_FONT_NORMAL, CELLPHONE_COLOR_PRIMARY);
    lv_obj_align(arrow, LV_ALIGN_LEFT_MID, 8, 0);

    lv_obj_add_event_cb(slider, slider_event_cb, LV_EVENT_ALL, NULL);

    return parent;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void slider_reset_anim_cb(void * var, int32_t value)
{
    lv_slider_set_value((lv_obj_t *)var, value, LV_ANIM_OFF);
}

static void hint_text_opa_cb(void * var, int32_t value)
{
    lv_obj_set_style_text_opa((lv_obj_t *)var, (lv_opa_t)value, 0);
}

static void slider_event_cb(lv_event_t * e)
{
    lv_obj_t * slider = lv_event_get_target(e);
    lv_event_code_t code = lv_event_get_code(e);
    int32_t val = lv_slider_get_value(slider);

    if(code == LV_EVENT_VALUE_CHANGED) {
        if(val >= 90 && !s_shutter_started) {
            /* Hiding the slider doesn't synchronously stop further
             * VALUE_CHANGED events that may already be queued, so latch
             * once we kick off the reveal -- otherwise a fast flick
             * could spawn duplicate panel pairs. */
            s_shutter_started = true;

            /* E8: Shutter reveal -- split the visible LCD at its midpoint,
             * but size the panels against the oversized lock parent so the
             * extra chrome-strip coverage remains hidden during teardown. */
            lv_obj_t * lock_parent = lv_obj_get_parent(slider);
            int32_t parent_h = lv_obj_get_height(lock_parent);
            int32_t split_y = CELLPHONE_CONTENT_Y + (CELLPHONE_VER_RES / 2);

            /* Top shutter panel */
            lv_obj_t * top_panel = cellphone_obj_fill(lock_parent, lv_color_hex(0x1a1a2e));
            lv_obj_set_pos(top_panel, 0, 0);
            lv_obj_set_size(top_panel, CELLPHONE_HOR_RES, split_y);
            lv_obj_clear_flag(top_panel, LV_OBJ_FLAG_SCROLLABLE);

            /* Bottom shutter panel */
            lv_obj_t * bot_panel = cellphone_obj_fill(lock_parent, lv_color_black());
            lv_obj_set_pos(bot_panel, 0, split_y);
            lv_obj_set_size(bot_panel, CELLPHONE_HOR_RES, parent_h - split_y);
            lv_obj_clear_flag(bot_panel, LV_OBJ_FLAG_SCROLLABLE);

            /* Animate top panel upward using the shared QUICK preset. */
            cellphone_anim_run(top_panel, cellphone_anim_set_y_cb,
                               0, -split_y, CELLPHONE_MOTION_QUICK.exit_ms,
                               CELLPHONE_MOTION_QUICK.path_cb);

            /* Animate bottom panel downward with an 80 ms delay so it
             * trails the top panel. Built inline rather than via
             * cellphone_anim_run because it carries delay + completed_cb,
             * which the shared helper doesn't expose. */
            lv_anim_t a;
            lv_anim_init(&a);
            lv_anim_set_var(&a, bot_panel);
            lv_anim_set_exec_cb(&a, cellphone_anim_set_y_cb);
            lv_anim_set_values(&a, split_y, parent_h);
            lv_anim_set_duration(&a, CELLPHONE_MOTION_QUICK.exit_ms);
            lv_anim_set_delay(&a, 80);
            lv_anim_set_path_cb(&a, CELLPHONE_MOTION_QUICK.path_cb);
            lv_anim_set_completed_cb(&a, shutter_done_cb);
            lv_anim_start(&a);

            /* Hide original content immediately so panels are the only
             * visible layer during the transition */
            lv_obj_add_flag(slider, LV_OBJ_FLAG_HIDDEN);
        }
    }
    else if(code == LV_EVENT_RELEASED) {
        if(val < 90) {
            /* Animate back to 0 */
            cellphone_anim_run(slider, slider_reset_anim_cb,
                               val, 0, CELLPHONE_MOTION_QUICK.enter_ms,
                               CELLPHONE_MOTION_QUICK.path_cb);
        }
    }
}

static void lock_clock_timer_cb(lv_timer_t * timer)
{
    LV_UNUSED(timer);
    if(!s_lock_clock || !s_lock_date) return;

    char buf[8];
    cellphone_time_now(buf, sizeof(buf));
    lv_label_set_text(s_lock_clock, buf);

    char dbuf[40];
    cellphone_date_now(dbuf, sizeof(dbuf));
    lv_label_set_text(s_lock_date, dbuf);
}

static void lock_delete_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    if(s_lock_timer) {
        lv_timer_delete(s_lock_timer);
        s_lock_timer = NULL;
    }
    s_lock_clock = NULL;
    s_lock_date = NULL;
}

static void shutter_done_cb(lv_anim_t * a)
{
    LV_UNUSED(a);
    cellphone_chrome_set_visible(true);
    cellphone_screen_pop();
}

#endif /* LV_USE_DEMO_CELLPHONE */
