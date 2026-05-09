/**
 * @file lv_demo_cellphone_photo.c
 *
 * Photo viewer: 3x2 thumbnail grid + fullscreen tileview browser.
 */

#include "lv_demo_cellphone_photo.h"
#include "lv_demo_cellphone_anim.h"

#if LV_USE_DEMO_CELLPHONE

/*********************
 *      DEFINES
 *********************/
#define PHOTO_COUNT     6
#define GRID_COLS       3
#define GRID_ROWS       2
#define THUMB_GAP       4

/**********************
 *  STATIC VARIABLES
 **********************/
static uint32_t s_selected_photo_idx;

/**********************
 *  STATIC PROTOTYPES
 **********************/
static lv_obj_t * photo_viewer_create(lv_obj_t * parent);
static void thumb_click_cb(lv_event_t * e);
static lv_color_t photo_color(uint32_t idx);
static void viewer_tap_cb(lv_event_t * e);
static void viewer_tile_changed_cb(lv_event_t * e);
static void update_counter_label(uint32_t photo_idx);
static void viewer_delete_cb(lv_event_t * e);

static lv_obj_t * s_viewer_title;
static lv_obj_t * s_viewer_counter;
static lv_obj_t * s_viewer_counter_label;
static bool s_fullscreen;

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

lv_obj_t * cellphone_photo_create(lv_obj_t * parent)
{
    /* Container fills the content area the screen stack provides. */
    lv_obj_set_size(parent, CELLPHONE_CONTENT_W, CELLPHONE_CONTENT_H);
    cellphone_obj_paint_fill(parent, CELLPHONE_COLOR_BG);
    lv_obj_set_style_pad_all(parent, THUMB_GAP, 0);

    /* Title */
    lv_obj_t * title = cellphone_label(parent, "Photos",
                                       CELLPHONE_FONT_HEADING, CELLPHONE_COLOR_TEXT);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 4, 0);

    /* Grid container below the title */
    lv_obj_t * grid = cellphone_obj_bare(parent);
    lv_obj_set_size(grid, CELLPHONE_CONTENT_W - THUMB_GAP * 2,
                    CELLPHONE_CONTENT_H - 24 - THUMB_GAP * 2);
    lv_obj_align(grid, LV_ALIGN_TOP_LEFT, 0, 22);
    lv_obj_set_layout(grid, LV_LAYOUT_GRID);

    /* Column / row descriptors -- equal-weight tracks */
    static const int32_t col_dsc[] = {
        LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST
    };
    static const int32_t row_dsc[] = {
        LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST
    };

    lv_obj_set_grid_dsc_array(grid, col_dsc, row_dsc);
    lv_obj_set_style_pad_column(grid, THUMB_GAP, 0);
    lv_obj_set_style_pad_row(grid, THUMB_GAP, 0);

    /* Create 6 placeholder thumbnails */
    uint32_t i;
    for(i = 0; i < PHOTO_COUNT; i++) {
        uint32_t col = i % GRID_COLS;
        uint32_t row = i / GRID_COLS;

        lv_obj_t * thumb = cellphone_obj_fill(grid, photo_color(i));
        lv_obj_set_style_radius(thumb, 6, 0);
        lv_obj_set_grid_cell(thumb,
                             LV_GRID_ALIGN_STRETCH, col, 1,
                             LV_GRID_ALIGN_STRETCH, row, 1);
        lv_obj_add_flag(thumb, LV_OBJ_FLAG_CLICKABLE);

        /* Number label centered on thumbnail */
        char buf[4];
        lv_snprintf(buf, sizeof(buf), "%" LV_PRIu32, i + 1);
        lv_obj_center(cellphone_label(thumb, buf,
                                      CELLPHONE_FONT_NORMAL, lv_color_white()));

        /* Carry the index in the event descriptor instead of per-object
         * user_data so thumbnails stay on the leanest object layout. */
        lv_obj_add_event_cb(thumb, thumb_click_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)i);
    }

    return parent;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static lv_color_t photo_color(uint32_t idx)
{
    /* Spread hues evenly: 0, 40, 80, 120, 160, 200 */
    return lv_color_hsv_to_rgb(idx * 40, 70, 80);
}

static void thumb_click_cb(lv_event_t * e)
{
    s_selected_photo_idx = (uint32_t)(uintptr_t)lv_event_get_user_data(e);
    cellphone_screen_push(photo_viewer_create);
}

static lv_obj_t * photo_viewer_create(lv_obj_t * parent)
{
    s_fullscreen = false;

    /* Black background, fill entire content area */
    lv_obj_set_size(parent, CELLPHONE_CONTENT_W, CELLPHONE_CONTENT_H);
    cellphone_obj_paint_fill(parent, lv_color_black());
    lv_obj_set_style_pad_all(parent, 0, 0);

    /* Title bar at top -- will slide off-screen in fullscreen mode */
    s_viewer_title = cellphone_obj_bare(parent);
    lv_obj_set_size(s_viewer_title, CELLPHONE_CONTENT_W, 28);
    lv_obj_set_pos(s_viewer_title, 0, 0);
    lv_obj_set_style_bg_color(s_viewer_title, lv_color_hex(0x1a1a1a), 0);
    lv_obj_set_style_bg_opa(s_viewer_title, LV_OPA_80, 0);
    lv_obj_clear_flag(s_viewer_title, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * title_lbl = cellphone_label(s_viewer_title, "Photos",
                                           CELLPHONE_FONT_SM, lv_color_white());
    lv_obj_align(title_lbl, LV_ALIGN_LEFT_MID, 8, 0);

    /* Counter bar at bottom */
    s_viewer_counter = cellphone_obj_bare(parent);
    lv_obj_set_size(s_viewer_counter, CELLPHONE_CONTENT_W, 24);
    lv_obj_align(s_viewer_counter, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(s_viewer_counter, lv_color_hex(0x1a1a1a), 0);
    lv_obj_set_style_bg_opa(s_viewer_counter, LV_OPA_80, 0);
    lv_obj_clear_flag(s_viewer_counter, LV_OBJ_FLAG_SCROLLABLE);

    s_viewer_counter_label = cellphone_label(s_viewer_counter, NULL,
                                             CELLPHONE_FONT_SM, lv_color_white());
    lv_obj_center(s_viewer_counter_label);
    update_counter_label(s_selected_photo_idx);

    /* Horizontal tileview for swipe navigation */
    lv_obj_t * tv = lv_tileview_create(parent);
    lv_obj_set_size(tv, CELLPHONE_CONTENT_W, CELLPHONE_CONTENT_H);
    cellphone_obj_paint_fill(tv, lv_color_black());

    /* Tileview behind controls */
    lv_obj_move_background(tv);

    uint32_t i;
    for(i = 0; i < PHOTO_COUNT; i++) {
        lv_obj_t * tile = lv_tileview_add_tile(tv, i, 0, LV_DIR_HOR);

        lv_obj_t * rect = cellphone_obj_fill(tile, photo_color(i));
        lv_obj_set_size(rect, CELLPHONE_CONTENT_W - 16, CELLPHONE_CONTENT_H - 16);
        lv_obj_center(rect);
        lv_obj_set_style_radius(rect, 8, 0);

        char buf[16];
        lv_snprintf(buf, sizeof(buf), "Photo %" LV_PRIu32, i + 1);
        lv_obj_center(cellphone_label(rect, buf,
                                      CELLPHONE_FONT_HEADING, lv_color_white()));

        /* Tap to toggle fullscreen */
        lv_obj_add_flag(rect, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(rect, viewer_tap_cb, LV_EVENT_CLICKED, parent);
    }

    lv_obj_add_event_cb(tv, viewer_tile_changed_cb, LV_EVENT_VALUE_CHANGED, NULL);
    if(s_selected_photo_idx >= PHOTO_COUNT) s_selected_photo_idx = 0;
    lv_tileview_set_tile_by_index(tv, s_selected_photo_idx, 0, LV_ANIM_OFF);

    /* Null statics on screen destruction to prevent dangling pointers */
    lv_obj_add_event_cb(parent, viewer_delete_cb, LV_EVENT_DELETE, NULL);

    return parent;
}

static void viewer_delete_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    s_viewer_title = NULL;
    s_viewer_counter = NULL;
    s_viewer_counter_label = NULL;
}

/** E14: Toggle fullscreen -- slide title/counter off-screen or back. */
static void viewer_tap_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    if(!s_viewer_title || !s_viewer_counter) return;

    int32_t title_target, counter_target;
    uint32_t dur;

    if(!s_fullscreen) {
        /* Enter fullscreen: slide title up, counter down */
        title_target = -28;
        counter_target = CELLPHONE_CONTENT_H;
        dur = 400;
    }
    else {
        /* Exit fullscreen: slide back */
        title_target = 0;
        counter_target = CELLPHONE_CONTENT_H - 24;
        dur = 300;
    }

    cellphone_anim_run(s_viewer_title, cellphone_anim_set_y_cb,
                       lv_obj_get_y(s_viewer_title), title_target,
                       dur, lv_anim_path_ease_out);
    cellphone_anim_run(s_viewer_counter, cellphone_anim_set_y_cb,
                       lv_obj_get_y(s_viewer_counter), counter_target,
                       dur, lv_anim_path_ease_out);

    s_fullscreen = !s_fullscreen;
}

static void viewer_tile_changed_cb(lv_event_t * e)
{
    lv_obj_t * tv = lv_event_get_target(e);
    lv_obj_t * tile = lv_tileview_get_tile_active(tv);

    if(tile == NULL) return;

    int32_t tv_w = lv_obj_get_content_width(tv);
    if(tv_w <= 0) return;

    s_selected_photo_idx = (uint32_t)(lv_obj_get_x(tile) / tv_w);
    update_counter_label(s_selected_photo_idx);
}

static void update_counter_label(uint32_t photo_idx)
{
    char cnt_buf[16];

    if(s_viewer_counter_label == NULL) return;

    lv_snprintf(cnt_buf, sizeof(cnt_buf), "%" LV_PRIu32 " / %d",
                photo_idx + 1, PHOTO_COUNT);
    lv_label_set_text(s_viewer_counter_label, cnt_buf);
}

#endif /* LV_USE_DEMO_CELLPHONE */
