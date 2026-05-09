/**
 * @file lv_demo_cellphone_music.c
 *
 * Music player app for the cellphone demo.
 *
 * Track browser is a plain list of titles; tapping a row opens the
 * "now playing" screen with title/artist/progress slider and transport
 * controls.  Album art is a procedural color block keyed off the
 * current track index, so the demo has zero baked-in cover bitmaps.
 */

#include "lv_demo_cellphone_music.h"
#include "lv_demo_cellphone_data.h"

#if LV_USE_DEMO_CELLPHONE

/*********************
 *  STATIC VARIABLES
 *********************/
static uint32_t s_current_track;
static uint32_t s_playback_pos;
static lv_timer_t * s_playback_timer;
static lv_obj_t * s_album_art;
static lv_obj_t * s_album_label;
static lv_obj_t * s_title_label;
static lv_obj_t * s_artist_label;
static lv_obj_t * s_slider;
static lv_obj_t * s_elapsed_label;
static lv_obj_t * s_duration_label;
static lv_obj_t * s_play_btn;

/*********************
 *  STATIC PROTOTYPES
 *********************/
static void format_time(uint32_t sec, char * buf, uint32_t buf_len);
static lv_color_t album_art_color(uint32_t track_idx);
static lv_obj_t * now_playing_create(lv_obj_t * parent);
static lv_obj_t * transport_button(lv_obj_t * row, const char * symbol,
                                   lv_event_cb_t cb);
static void update_now_playing(void);
static void track_btn_cb(lv_event_t * e);
static void switch_track(uint32_t idx);
static void play_pause_cb(lv_event_t * e);
static void prev_cb(lv_event_t * e);
static void next_cb(lv_event_t * e);
static void timer_cb(lv_timer_t * t);
static void now_playing_delete_cb(lv_event_t * e);

/*********************
 *   GLOBAL FUNCTIONS
 *********************/
lv_obj_t * cellphone_music_create(lv_obj_t * parent)
{
    const cellphone_track_t * tracks = cellphone_data_tracks();

    lv_obj_t * list = lv_list_create(parent);
    lv_obj_set_size(list, CELLPHONE_CONTENT_W, CELLPHONE_CONTENT_H);
    lv_obj_set_style_bg_color(list, CELLPHONE_COLOR_BG, 0);
    lv_list_add_text(list, "Music");

    uint32_t i;
    for(i = 0; i < CELLPHONE_TRACK_COUNT; i++) {
        char buf[128];
        lv_snprintf(buf, sizeof(buf), LV_SYMBOL_AUDIO " %s - %s",
                    tracks[i].title, tracks[i].artist);
        lv_obj_t * btn = lv_list_add_button(list, NULL, buf);
        lv_obj_add_event_cb(btn, track_btn_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)i);
    }

    return parent;
}

/*********************
 *   STATIC FUNCTIONS
 *********************/
static void format_time(uint32_t sec, char * buf, uint32_t buf_len)
{
    lv_snprintf(buf, buf_len, "%" LV_PRIu32 ":%02" LV_PRIu32, sec / 60, sec % 60);
}

static lv_color_t album_art_color(uint32_t track_idx)
{
    uint16_t hue = (uint16_t)((track_idx * 60U) % 360U);
    return lv_color_hsv_to_rgb(hue, 50, 70);
}

static lv_obj_t * transport_button(lv_obj_t * row, const char * symbol,
                                   lv_event_cb_t cb)
{
    lv_obj_t * btn = lv_button_create(row);
    lv_obj_set_size(btn, CELLPHONE_BTN_MIN, CELLPHONE_BTN_MIN);
    lv_obj_set_style_bg_color(btn, CELLPHONE_COLOR_PRIMARY, 0);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);

    lv_obj_center(cellphone_label(btn, symbol, CELLPHONE_FONT_NORMAL, lv_color_white()));

    return btn;
}

static void track_btn_cb(lv_event_t * e)
{
    s_current_track = (uint32_t)(uintptr_t)lv_event_get_user_data(e);
    s_playback_pos = 0;
    cellphone_screen_push(now_playing_create);
}

static void update_now_playing(void)
{
    if(s_current_track >= CELLPHONE_TRACK_COUNT) return;
    if(s_album_art == NULL || s_album_label == NULL || s_title_label == NULL ||
       s_artist_label == NULL || s_slider == NULL ||
       s_elapsed_label == NULL || s_duration_label == NULL) return;

    const cellphone_track_t * tracks = cellphone_data_tracks();
    const cellphone_track_t * t = &tracks[s_current_track];
    char buf[32];

    lv_obj_set_style_bg_color(s_album_art, album_art_color(s_current_track), 0);
    lv_label_set_text(s_album_label, t->album);
    lv_label_set_text(s_title_label, t->title);
    lv_label_set_text(s_artist_label, t->artist);

    lv_slider_set_range(s_slider, 0, (int32_t)t->duration_sec);
    lv_slider_set_value(s_slider, (int32_t)s_playback_pos, LV_ANIM_OFF);

    format_time(s_playback_pos, buf, sizeof(buf));
    lv_label_set_text(s_elapsed_label, buf);

    format_time(t->duration_sec, buf, sizeof(buf));
    lv_label_set_text(s_duration_label, buf);
}

/*
 * Central track-switch: update state, refresh UI,
 * and ensure playback is running.
 */
static void switch_track(uint32_t idx)
{
    s_current_track = idx;
    s_playback_pos = 0;
    update_now_playing();

    if(s_play_btn != NULL) {
        lv_label_set_text(lv_obj_get_child(s_play_btn, 0), LV_SYMBOL_PAUSE);
    }
    if(s_playback_timer != NULL && lv_timer_get_paused(s_playback_timer)) {
        lv_timer_resume(s_playback_timer);
    }
}

static void timer_cb(lv_timer_t * t)
{
    LV_UNUSED(t);
    if(s_current_track >= CELLPHONE_TRACK_COUNT) return;
    if(s_slider == NULL || s_elapsed_label == NULL) return;

    const cellphone_track_t * tracks = cellphone_data_tracks();

    s_playback_pos++;
    if(s_playback_pos >= tracks[s_current_track].duration_sec) {
        switch_track((s_current_track + 1) % CELLPHONE_TRACK_COUNT);
        return;
    }

    lv_slider_set_value(s_slider, (int32_t)s_playback_pos, LV_ANIM_ON);

    char buf[32];
    format_time(s_playback_pos, buf, sizeof(buf));
    lv_label_set_text(s_elapsed_label, buf);
}

static void play_pause_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    if(s_playback_timer == NULL) return;

    if(lv_timer_get_paused(s_playback_timer)) {
        lv_timer_resume(s_playback_timer);
        lv_label_set_text(lv_obj_get_child(s_play_btn, 0), LV_SYMBOL_PAUSE);
    }
    else {
        lv_timer_pause(s_playback_timer);
        lv_label_set_text(lv_obj_get_child(s_play_btn, 0), LV_SYMBOL_PLAY);
    }
}

static void prev_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    uint32_t idx = (s_current_track == 0) ? CELLPHONE_TRACK_COUNT - 1 : s_current_track - 1;
    switch_track(idx);
}

static void next_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    switch_track((s_current_track + 1) % CELLPHONE_TRACK_COUNT);
}

static void now_playing_delete_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    s_album_art     = NULL;
    s_album_label   = NULL;
    s_title_label   = NULL;
    s_artist_label  = NULL;
    s_slider        = NULL;
    s_elapsed_label = NULL;
    s_duration_label = NULL;
    s_play_btn      = NULL;
    if(s_playback_timer != NULL) {
        lv_timer_delete(s_playback_timer);
        s_playback_timer = NULL;
    }
}

static lv_obj_t * now_playing_create(lv_obj_t * parent)
{
    if(s_current_track >= CELLPHONE_TRACK_COUNT) return parent;

    const cellphone_track_t * tracks = cellphone_data_tracks();
    const cellphone_track_t * t = &tracks[s_current_track];

    cellphone_obj_paint_fill(parent, CELLPHONE_COLOR_BG);
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(parent, 10, 0);
    lv_obj_set_style_pad_row(parent, 6, 0);

    lv_obj_add_event_cb(parent, now_playing_delete_cb, LV_EVENT_DELETE, NULL);

    /* album art -- procedural color block keyed off the track index */
    {
        int32_t art_size = CELLPHONE_CONTENT_W / 2;

        s_album_art = cellphone_obj_fill(parent, album_art_color(s_current_track));
        lv_obj_set_size(s_album_art, art_size, art_size);
        lv_obj_set_style_radius(s_album_art, 12, 0);
        lv_obj_clear_flag(s_album_art, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t * note_lbl = cellphone_label(s_album_art, LV_SYMBOL_AUDIO,
                                              CELLPHONE_FONT_CLOCK, lv_color_white());
        lv_obj_set_style_text_opa(note_lbl, LV_OPA_60, 0);
        lv_obj_center(note_lbl);
    }

    /* album name */
    s_album_label = cellphone_label(parent, t->album,
                                    CELLPHONE_FONT_SM, CELLPHONE_COLOR_TEXT_SEC);

    /* track title */
    s_title_label = cellphone_label(parent, t->title,
                                    CELLPHONE_FONT_HEADING, CELLPHONE_COLOR_TEXT);

    /* artist */
    s_artist_label = cellphone_label(parent, t->artist,
                                     CELLPHONE_FONT_NORMAL, CELLPHONE_COLOR_TEXT_SEC);

    /* progress slider */
    s_slider = lv_slider_create(parent);
    lv_obj_set_width(s_slider, CELLPHONE_CONTENT_W - 40);
    lv_obj_set_style_bg_color(s_slider, CELLPHONE_COLOR_PRIMARY, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(s_slider, CELLPHONE_COLOR_PRIMARY, LV_PART_KNOB);
    lv_slider_set_range(s_slider, 0, (int32_t)t->duration_sec);
    lv_slider_set_value(s_slider, 0, LV_ANIM_OFF);
    lv_obj_remove_flag(s_slider, LV_OBJ_FLAG_CLICKABLE);

    /* time labels row */
    lv_obj_t * time_row = cellphone_obj_bare(parent);
    lv_obj_set_size(time_row, CELLPHONE_CONTENT_W - 40, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(time_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(time_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    char buf[32];

    s_elapsed_label = cellphone_label(time_row, "0:00",
                                      CELLPHONE_FONT_SM, CELLPHONE_COLOR_TEXT_SEC);

    format_time(t->duration_sec, buf, sizeof(buf));
    s_duration_label = cellphone_label(time_row, buf,
                                       CELLPHONE_FONT_SM, CELLPHONE_COLOR_TEXT_SEC);

    /* control buttons row */
    lv_obj_t * ctrl_row = cellphone_obj_bare(parent);
    lv_obj_set_size(ctrl_row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(ctrl_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(ctrl_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(ctrl_row, 20, 0);

    transport_button(ctrl_row, LV_SYMBOL_PREV, prev_cb);
    s_play_btn = transport_button(ctrl_row, LV_SYMBOL_PAUSE, play_pause_cb);
    transport_button(ctrl_row, LV_SYMBOL_NEXT, next_cb);

    s_playback_timer = lv_timer_create(timer_cb, 1000, NULL);

    return parent;
}

#endif
