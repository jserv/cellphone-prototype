/**
 * @file lv_demo_cellphone_statusbar.c
 *
 * Persistent top status bar: clock, battery icon, signal icon.
 */

#include "lv_demo_cellphone_statusbar.h"

#if LV_USE_DEMO_CELLPHONE

static lv_obj_t * s_clock_label;
static lv_timer_t * s_clock_timer;
static void statusbar_draw_cb(lv_event_t * e);

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

static void statusbar_draw_cb(lv_event_t * e)
{
    lv_obj_t * bar = lv_event_get_target(e);
    lv_layer_t * layer = lv_event_get_layer(e);
    lv_area_t coords;
    lv_area_t a;
    lv_draw_rect_dsc_t rect;
    lv_color_t fg = CELLPHONE_COLOR_TEXT;
    lv_color_t dim = lv_color_mix(fg, CELLPHONE_COLOR_STATUSBAR, LV_OPA_40);

    lv_obj_get_coords(bar, &coords);
    lv_draw_rect_dsc_init(&rect);
    rect.bg_opa = LV_OPA_COVER;
    rect.radius = 1;

    /* Classic feature-phone signal bars on the left. */
    static const int8_t signal_heights[] = { 4, 7, 10, 13 };
    int32_t sig_x = coords.x1 + 8;
    int32_t sig_base = coords.y2 - 3;
    for(uint32_t i = 0; i < 4; i++) {
        rect.bg_color = i < 3 ? fg : dim;
        a.x1 = sig_x + (int32_t)i * 4;
        a.x2 = a.x1 + 2;
        a.y2 = sig_base;
        a.y1 = sig_base - signal_heights[i] + 1;
        lv_draw_rect(layer, &rect, &a);
    }

    /* Battery frame on the right with a filled charge body. */
    rect.bg_opa = LV_OPA_TRANSP;
    rect.border_width = 2;
    rect.border_color = fg;
    rect.radius = 2;
    a.x2 = coords.x2 - 9;
    a.x1 = a.x2 - 15;
    a.y1 = coords.y1 + 5;
    a.y2 = a.y1 + 8;
    lv_draw_rect(layer, &rect, &a);

    rect.bg_opa = LV_OPA_COVER;
    rect.bg_color = fg;
    rect.border_width = 0;
    rect.radius = 1;
    a.x1 = coords.x2 - 8;
    a.x2 = a.x1 + 1;
    a.y1 = coords.y1 + 7;
    a.y2 = a.y1 + 4;
    lv_draw_rect(layer, &rect, &a);

    a.x1 = coords.x2 - 22;
    a.x2 = coords.x2 - 12;
    a.y1 = coords.y1 + 7;
    a.y2 = a.y1 + 4;
    lv_draw_rect(layer, &rect, &a);

    /* Thin gloss/separator lines reinforce the feature-phone chrome. */
    rect.bg_color = lv_color_mix(lv_color_white(), CELLPHONE_COLOR_STATUSBAR, LV_OPA_20);
    rect.radius = 0;
    a.x1 = coords.x1;
    a.x2 = coords.x2;
    a.y1 = coords.y1;
    a.y2 = coords.y1;
    lv_draw_rect(layer, &rect, &a);

    rect.bg_color = lv_color_mix(CELLPHONE_COLOR_TEXT_SEC, CELLPHONE_COLOR_STATUSBAR_GRAD, LV_OPA_40);
    a.y1 = coords.y2;
    a.y2 = coords.y2;
    lv_draw_rect(layer, &rect, &a);
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
     * Keep the clock as the only child object and paint the chrome glyphs
     * directly for a lighter, more feature-phone-like presentation. */
    char time_buf[8];
    cellphone_time_now(time_buf, sizeof(time_buf));
    s_clock_label = cellphone_label(bar, time_buf, CELLPHONE_FONT_SM, CELLPHONE_COLOR_TEXT);
    lv_obj_align(s_clock_label, LV_ALIGN_CENTER, 0, 0);

    /* Timer to update clock once per minute */
    s_clock_timer = lv_timer_create(clock_timer_cb, 60000, NULL);

    lv_obj_add_event_cb(bar, statusbar_draw_cb, LV_EVENT_DRAW_MAIN, NULL);
    lv_obj_add_event_cb(bar, statusbar_delete_cb, LV_EVENT_DELETE, NULL);
}

#endif /* LV_USE_DEMO_CELLPHONE */
