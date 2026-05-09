/**
 * @file lv_demo_cellphone_statusbar.c
 *
 * Persistent top status bar: clock, battery icon, signal icon.
 */

#include "lv_demo_cellphone_statusbar.h"

#if LV_USE_DEMO_CELLPHONE

static lv_obj_t * s_clock_label;
static lv_timer_t * s_clock_timer;

static void statusbar_delete_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    if(s_clock_timer) {
        lv_timer_delete(s_clock_timer);
        s_clock_timer = NULL;
    }
    s_clock_label = NULL;
}

static void clock_timer_cb(lv_timer_t * timer)
{
    LV_UNUSED(timer);
    if(!s_clock_label) return;
    char buf[8];
    cellphone_time_now(buf, sizeof(buf));
    lv_label_set_text(s_clock_label, buf);
}

void cellphone_statusbar_create(lv_obj_t * layer)
{
    if(s_clock_timer) {
        lv_timer_delete(s_clock_timer);
        s_clock_timer = NULL;
    }

    /* Vertical-gradient bar: darker at top, lighter at bottom, matching
     * the glossy retro-phone look in the reference UI. */
    lv_obj_t * bar = cellphone_obj_bar(layer, CELLPHONE_COLOR_STATUSBAR_GRAD,
                                       CELLPHONE_COLOR_STATUSBAR);
    lv_obj_set_pos(bar, 0, 0);
    lv_obj_set_size(bar, CELLPHONE_HOR_RES, CELLPHONE_STATUSBAR_H);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

    /* Reference UI lays out the bar as: signal | (centered) time | battery.
     * The clock anchors the middle; signal and battery hug the edges. */
    char time_buf[8];
    cellphone_time_now(time_buf, sizeof(time_buf));
    s_clock_label = cellphone_label(bar, time_buf, CELLPHONE_FONT_SM, CELLPHONE_COLOR_TEXT);
    lv_obj_align(s_clock_label, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t * signal = cellphone_label(bar, LV_SYMBOL_WIFI,
                                        CELLPHONE_FONT_SM, CELLPHONE_COLOR_TEXT);
    lv_obj_align(signal, LV_ALIGN_LEFT_MID, 8, 0);

    lv_obj_t * batt = cellphone_label(bar, LV_SYMBOL_BATTERY_FULL,
                                      CELLPHONE_FONT_SM, CELLPHONE_COLOR_TEXT);
    lv_obj_align(batt, LV_ALIGN_RIGHT_MID, -8, 0);

    /* Timer to update clock once per minute */
    s_clock_timer = lv_timer_create(clock_timer_cb, 60000, NULL);

    lv_obj_add_event_cb(bar, statusbar_delete_cb, LV_EVENT_DELETE, NULL);
}

#endif /* LV_USE_DEMO_CELLPHONE */
