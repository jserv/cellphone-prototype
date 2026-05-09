/**
 * @file lv_demo_cellphone_home.c
 *
 * Home screen: paginated icon grid using lv_tileview.
 * Page indicator dots at the bottom.
 */

#include "lv_demo_cellphone_home.h"
#include "lv_demo_cellphone_anim.h"

#if LV_USE_DEMO_CELLPHONE

/*********************
 *      DEFINES
 *********************/
#define INDICATOR_DOT_SIZE   6
#define INDICATOR_DOT_GAP    8
#define INDICATOR_MARGIN_BOT 4

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void icon_event_cb(lv_event_t * e);
static void page_changed_cb(lv_event_t * e);
static lv_obj_t * create_icon(lv_obj_t * parent, const char * label_text,
                              const void * icon_src,
                              cellphone_screen_create_fn fn);
static void icon_wobble_all(uint32_t stagger_ms);
static void icon_wobble_stop_all(void);

/**********************
 *  STATIC VARIABLES
 **********************/
static lv_obj_t * s_dots[4];  /* max 4 pages */
static uint32_t   s_page_count;
static bool       s_edit_mode;
static bool       s_ignore_release_click;
static lv_obj_t * s_picked_icon;
static lv_obj_t * s_tileview;

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

lv_obj_t * cellphone_home_create(lv_obj_t * parent)
{
    s_edit_mode = false;
    s_ignore_release_click = false;
    s_picked_icon = NULL;
    s_tileview = NULL;

    uint32_t app_count;
    const cellphone_app_entry_t * apps = cellphone_app_registry(&app_count);

    /* Use compile-time constants instead of lv_obj_get_width/height
     * to avoid zero values when layout hasn't resolved yet. */
    int32_t content_w = CELLPHONE_CONTENT_W;
    int32_t content_h = CELLPHONE_CONTENT_H;

    /* Compute number of pages needed */
    uint32_t icons_per_page = CELLPHONE_ICONS_PER_PAGE;
    s_page_count = (app_count + icons_per_page - 1) / icons_per_page;
    if(s_page_count > 4) s_page_count = 4;
    if(s_page_count == 0) s_page_count = 1;

    /* Tileview for horizontal paging */
    lv_obj_t * tv = lv_tileview_create(parent);
    s_tileview = tv;
    lv_obj_set_size(tv, content_w, content_h - INDICATOR_DOT_SIZE - INDICATOR_MARGIN_BOT * 2);
    lv_obj_set_pos(tv, 0, 0);
    lv_obj_set_style_bg_opa(tv, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(tv, 0, 0);
    lv_obj_set_style_border_width(tv, 0, 0);
    lv_obj_set_scrollbar_mode(tv, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_flag(tv, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

    uint32_t app_idx = 0;

    /* Grid descriptors: fractional units distribute space evenly,
     * eliminating left/right asymmetry from integer division remainders. */
    static const int32_t col_dsc[] = {
        LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1),
#if CELLPHONE_GRID_COLS > 3
        LV_GRID_FR(1),
#endif
        LV_GRID_TEMPLATE_LAST
    };
    static const int32_t row_dsc[] = {
        LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1),
#if CELLPHONE_GRID_ROWS > 3
        LV_GRID_FR(1),
#endif
        LV_GRID_TEMPLATE_LAST
    };

    for(uint32_t p = 0; p < s_page_count; p++) {
        lv_obj_t * tile = lv_tileview_add_tile(tv, p, 0, LV_DIR_HOR);
        lv_obj_set_style_bg_opa(tile, LV_OPA_TRANSP, 0);
        lv_obj_set_style_pad_all(tile, 0, 0);
        lv_obj_set_style_border_width(tile, 0, 0);

        lv_obj_set_grid_dsc_array(tile, col_dsc, row_dsc);

        for(int r = 0; r < CELLPHONE_GRID_ROWS && app_idx < app_count; r++) {
            for(int c = 0; c < CELLPHONE_GRID_COLS && app_idx < app_count; c++) {
                lv_obj_t * icon = create_icon(tile,
                                              apps[app_idx].name,
                                              apps[app_idx].icon,
                                              apps[app_idx].create);
                lv_obj_set_grid_cell(icon,
                                     LV_GRID_ALIGN_CENTER, c, 1,
                                     LV_GRID_ALIGN_CENTER, r, 1);
                app_idx++;
            }
        }
    }

    /* Page indicator dots */
    lv_obj_t * dot_row = cellphone_obj_bare(parent);
    int32_t dot_total_w = (int32_t)s_page_count * INDICATOR_DOT_SIZE
                          + ((int32_t)s_page_count - 1) * INDICATOR_DOT_GAP;
    lv_obj_set_size(dot_row, dot_total_w,
                    INDICATOR_DOT_SIZE + INDICATOR_MARGIN_BOT);
    lv_obj_align(dot_row, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_flex_flow(dot_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(dot_row, INDICATOR_DOT_GAP, 0);
    lv_obj_clear_flag(dot_row, LV_OBJ_FLAG_SCROLLABLE);

    for(uint32_t d = 0; d < s_page_count; d++) {
        lv_color_t dot_color = d == 0 ? CELLPHONE_COLOR_PRIMARY
                               : CELLPHONE_COLOR_INDICATOR;
        lv_obj_t * dot = cellphone_obj_fill(dot_row, dot_color);
        lv_obj_set_size(dot, INDICATOR_DOT_SIZE, INDICATOR_DOT_SIZE);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
        s_dots[d] = dot;
    }

    /* Listen for page changes */
    lv_obj_add_event_cb(tv, page_changed_cb, LV_EVENT_VALUE_CHANGED, NULL);

    return parent;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static lv_obj_t * create_icon(lv_obj_t * parent, const char * label_text,
                              const void * icon_src,
                              cellphone_screen_create_fn fn)
{
    lv_obj_t * cont = cellphone_obj_bare(parent);
    lv_obj_set_size(cont, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(cont, 4, 0);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(cont, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_OVERFLOW_VISIBLE);

    /* Icon: either a real image or a colored placeholder square */
    if(icon_src) {
        lv_obj_t * img = lv_image_create(cont);
        lv_image_set_src(img, icon_src);
        lv_obj_set_size(img, CELLPHONE_ICON_SIZE, CELLPHONE_ICON_SIZE);
    }
    else {
        /* Placeholder: colored rounded square with first letter.
         * Hash the label to pick a deterministic color so a given app
         * always renders the same swatch. */
        uint32_t hash = 0;
        for(const char * p = label_text; *p; p++) hash = hash * 31 + (uint32_t) * p;
        uint8_t hue = (uint8_t)(hash % 256);

        lv_obj_t * placeholder = cellphone_obj_fill(cont, lv_color_hsv_to_rgb(hue, 60, 85));
        lv_obj_set_size(placeholder, CELLPHONE_ICON_SIZE, CELLPHONE_ICON_SIZE);
        lv_obj_set_style_radius(placeholder, 8, 0);
        lv_obj_clear_flag(placeholder, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

        char buf[2] = { label_text[0], '\0' };
        lv_obj_center(cellphone_label(placeholder, buf,
                                      CELLPHONE_FONT_HEADING, lv_color_white()));
    }

    /* Label below icon */
    lv_obj_t * lbl = cellphone_label(cont, label_text,
                                     CELLPHONE_FONT_SM, CELLPHONE_COLOR_TEXT);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);

    /* One callback handles both click and long-press, avoiding a second
     * event descriptor per icon. */
    lv_obj_add_event_cb(cont, icon_event_cb, LV_EVENT_ALL, (void *)fn);

    return cont;
}

static void icon_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if(code == LV_EVENT_CLICKED) {
        if(s_ignore_release_click) {
            s_ignore_release_click = false;
            return;
        }

        /* If in edit mode, tapping an icon exits edit mode instead of launching */
        if(s_edit_mode) {
            icon_wobble_stop_all();
            if(s_picked_icon) {
                cellphone_anim_drop(s_picked_icon);
                s_picked_icon = NULL;
            }
            s_edit_mode = false;
            return;
        }

        cellphone_screen_create_fn fn =
            (cellphone_screen_create_fn)lv_event_get_user_data(e);
        if(fn) cellphone_screen_push(fn);
        return;
    }

    if(code != LV_EVENT_LONG_PRESSED) return;

    if(s_ignore_release_click) {
        s_ignore_release_click = false;
        return;
    }

    if(s_edit_mode) {
        s_ignore_release_click = true;
        return;
    }

    s_edit_mode = true;
    s_ignore_release_click = true;

    lv_obj_t * pressed = lv_event_get_target(e);
    icon_wobble_all(50);

    /* E3: Pickup effect on the long-pressed icon */
    s_picked_icon = pressed;
    cellphone_anim_pickup(pressed);
}

static void icon_wobble_all(uint32_t stagger_ms)
{
    if(!s_tileview) return;

    uint32_t icon_idx = 0;
    uint32_t tile_count = lv_obj_get_child_count(s_tileview);
    for(uint32_t t = 0; t < tile_count; t++) {
        lv_obj_t * tile = lv_obj_get_child(s_tileview, t);
        uint32_t child_count = lv_obj_get_child_count(tile);
        for(uint32_t i = 0; i < child_count; i++) {
            cellphone_anim_wobble_start(lv_obj_get_child(tile, i), icon_idx * stagger_ms);
            icon_idx++;
        }
    }
}

static void icon_wobble_stop_all(void)
{
    if(!s_tileview) return;

    uint32_t tile_count = lv_obj_get_child_count(s_tileview);
    for(uint32_t t = 0; t < tile_count; t++) {
        lv_obj_t * tile = lv_obj_get_child(s_tileview, t);
        uint32_t child_count = lv_obj_get_child_count(tile);
        for(uint32_t i = 0; i < child_count; i++) {
            cellphone_anim_wobble_stop(lv_obj_get_child(tile, i));
        }
    }
}

static void page_changed_cb(lv_event_t * e)
{
    lv_obj_t * tv = lv_event_get_target(e);
    lv_obj_t * tile = lv_tileview_get_tile_active(tv);
    if(!tile) return;

    int32_t tv_w = lv_obj_get_width(tv);
    if(tv_w <= 0) return;
    int32_t col = lv_obj_get_x(tile) / tv_w;

    for(uint32_t d = 0; d < s_page_count; d++) {
        lv_obj_set_style_bg_color(s_dots[d],
                                  (int32_t)d == col
                                  ? CELLPHONE_COLOR_PRIMARY
                                  : CELLPHONE_COLOR_INDICATOR,
                                  0);
    }
}

#endif /* LV_USE_DEMO_CELLPHONE */
