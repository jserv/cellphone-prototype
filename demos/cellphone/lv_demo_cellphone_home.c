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
#define INDICATOR_ACTIVE_W   18
#define INDICATOR_DOT_GAP    8
#define INDICATOR_MARGIN_BOT 4
#define ICON_LABEL_W         64
#define ICON_PLATE_SIZE      (CELLPHONE_ICON_SIZE + 12)
#define ICON_PLATE_Y         0
#define ICON_LABEL_Y         (ICON_PLATE_SIZE + 6)

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void icon_event_cb(lv_event_t * e);
static void page_changed_cb(lv_event_t * e);
static lv_obj_t * create_icon(lv_obj_t * parent, const char * label_text,
                              const void * icon_src,
                              cellphone_screen_create_fn fn);
static void create_camera_icon(lv_obj_t * parent, lv_color_t accent);
static void create_arcade_icon(lv_obj_t * parent, const char * label_text);
static void arcade_icon_draw_cb(lv_event_t * e);
static void indicator_set_active(uint32_t active_idx, bool animated);

/**********************
 *  STATIC VARIABLES
 **********************/
static lv_obj_t * s_dots[4];  /* max 4 pages */
static uint32_t   s_page_count;
static bool       s_edit_mode;
static bool       s_ignore_release_click;
static lv_obj_t * s_picked_icon;
static lv_obj_t * s_tileview;
static uint32_t   s_restore_page;

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

    /* Page indicator dots. Worst-case width assumes one dot is animated to
     * INDICATOR_ACTIVE_W while the rest stay at INDICATOR_DOT_SIZE; sizing
     * for the steady state would clip the active pill on every swipe. */
    lv_obj_t * dot_row = cellphone_obj_bare(parent);
    int32_t dot_total_w = INDICATOR_ACTIVE_W
                          + ((int32_t)s_page_count - 1) * INDICATOR_DOT_SIZE
                          + ((int32_t)s_page_count - 1) * INDICATOR_DOT_GAP;
    lv_obj_set_size(dot_row, dot_total_w,
                    INDICATOR_DOT_SIZE + INDICATOR_MARGIN_BOT);
    lv_obj_align(dot_row, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_flex_flow(dot_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(dot_row, INDICATOR_DOT_GAP, 0);
    lv_obj_clear_flag(dot_row, LV_OBJ_FLAG_SCROLLABLE);

    for(uint32_t d = 0; d < s_page_count; d++) {
        lv_obj_t * dot = cellphone_obj_fill(dot_row, CELLPHONE_COLOR_INDICATOR);
        lv_obj_set_size(dot, INDICATOR_DOT_SIZE, INDICATOR_DOT_SIZE);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(dot, LV_OPA_70, 0);
        s_dots[d] = dot;
    }
    indicator_set_active(0, false);

    /* Listen for page changes */
    lv_obj_add_event_cb(tv, page_changed_cb, LV_EVENT_VALUE_CHANGED, NULL);

    if(s_restore_page < s_page_count) {
        lv_tileview_set_tile_by_index(tv, s_restore_page, 0, LV_ANIM_OFF);
        indicator_set_active(s_restore_page, false);
    }
    s_restore_page = 0;

    return parent;
}

uint32_t cellphone_home_refresh_state_capture(void)
{
    lv_obj_t * tile;
    int32_t tv_w;

    if(!s_tileview || !lv_obj_is_valid(s_tileview)) return 0;

    tile = lv_tileview_get_tile_active(s_tileview);
    if(!tile) return 0;

    tv_w = lv_obj_get_width(s_tileview);
    if(tv_w <= 0) return 0;

    return (uint32_t)(lv_obj_get_x(tile) / tv_w);
}

void cellphone_home_refresh_state_restore(uint32_t state)
{
    s_restore_page = state;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static lv_obj_t * create_icon(lv_obj_t * parent, const char * label_text,
                              const void * icon_src,
                              cellphone_screen_create_fn fn)
{
    lv_obj_t * cont = cellphone_obj_bare(parent);
    lv_obj_set_size(cont, ICON_LABEL_W + 12, CELLPHONE_ICON_SIZE + CELLPHONE_ICON_LABEL_H + 18);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(cont, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_OVERFLOW_VISIBLE);

    /* Icon: either a real image or a colored placeholder square */
    if(icon_src) {
        lv_obj_t * img = lv_image_create(cont);
        lv_image_set_src(img, icon_src);
        lv_obj_align(img, LV_ALIGN_TOP_MID, 0, ICON_PLATE_Y);
    }
    else {
        if(lv_strcmp(label_text, "Camera") == 0 ||
           lv_strcmp(label_text, "Snake") == 0 ||
           lv_strcmp(label_text, "Pong") == 0 ||
           lv_strcmp(label_text, "Tetris") == 0) {
            if(lv_strcmp(label_text, "Camera") == 0) {
                lv_obj_t * icon_card = cellphone_obj_bare(cont);
                lv_obj_set_size(icon_card, ICON_PLATE_SIZE, ICON_PLATE_SIZE);
                lv_obj_align(icon_card, LV_ALIGN_TOP_MID, 0, ICON_PLATE_Y);
                lv_obj_clear_flag(icon_card, LV_OBJ_FLAG_SCROLLABLE);
                lv_obj_remove_flag(icon_card, LV_OBJ_FLAG_CLICKABLE);
                create_camera_icon(icon_card, lv_color_make(0x8c, 0x67, 0x52));
            }
            else {
                create_arcade_icon(cont, label_text);
            }
        }
        else {
            char buf[2] = { label_text[0], '\0' };
            lv_obj_t * placeholder = cellphone_label(cont, buf,
                                                     CELLPHONE_FONT_HEADING, CELLPHONE_COLOR_TEXT);
            lv_obj_align(placeholder, LV_ALIGN_TOP_MID, 0, 10);
        }
    }

    /* Label below icon */
    lv_obj_t * lbl = cellphone_label(cont, label_text,
                                     CELLPHONE_FONT_SM, CELLPHONE_COLOR_TEXT);
    lv_obj_set_width(lbl, ICON_LABEL_W);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_CLIP);
    lv_obj_align(lbl, LV_ALIGN_TOP_MID, 0, ICON_LABEL_Y);

    /* One callback handles both click and long-press, avoiding a second
     * event descriptor per icon. */
    lv_obj_add_event_cb(cont, icon_event_cb, LV_EVENT_ALL, (void *)fn);

    return cont;
}

static void create_camera_icon(lv_obj_t * parent, lv_color_t accent)
{
    lv_obj_t * body = cellphone_obj_fill(parent, accent);
    lv_obj_set_size(body, 30, 20);
    lv_obj_center(body);
    lv_obj_set_style_radius(body, 6, 0);
    lv_obj_remove_flag(body, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t * top = cellphone_obj_fill(body, lv_color_mix(accent, lv_color_white(), LV_OPA_20));
    lv_obj_set_size(top, 12, 5);
    lv_obj_align(top, LV_ALIGN_TOP_LEFT, 4, -3);
    lv_obj_set_style_radius(top, 3, 0);
    lv_obj_remove_flag(top, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t * lens_outer = cellphone_obj_fill(body, lv_color_hex(0x263238));
    lv_obj_set_size(lens_outer, 12, 12);
    lv_obj_center(lens_outer);
    lv_obj_set_style_radius(lens_outer, LV_RADIUS_CIRCLE, 0);
    lv_obj_remove_flag(lens_outer, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t * lens_inner = cellphone_obj_fill(lens_outer, lv_color_hex(0x90caf9));
    lv_obj_set_size(lens_inner, 6, 6);
    lv_obj_center(lens_inner);
    lv_obj_set_style_radius(lens_inner, LV_RADIUS_CIRCLE, 0);
    lv_obj_remove_flag(lens_inner, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t * flash = cellphone_obj_fill(body, lv_color_hex(0xfff59d));
    lv_obj_set_size(flash, 4, 4);
    lv_obj_align(flash, LV_ALIGN_RIGHT_MID, -4, 0);
    lv_obj_set_style_radius(flash, LV_RADIUS_CIRCLE, 0);
    lv_obj_remove_flag(flash, LV_OBJ_FLAG_CLICKABLE);
}

static void create_arcade_icon(lv_obj_t * parent, const char * label_text)
{
    lv_obj_add_event_cb(parent, arcade_icon_draw_cb, LV_EVENT_DRAW_MAIN, (void *)(uintptr_t)(label_text[0]));
}

static void arcade_icon_draw_cb(lv_event_t * e)
{
    lv_obj_t * obj = lv_event_get_target(e);
    lv_layer_t * layer = lv_event_get_layer(e);
    uintptr_t kind = (uintptr_t)lv_event_get_user_data(e);
    lv_color_t accent = kind == 'S' ? lv_color_make(0x3f, 0x9b, 0x64)
                        : kind == 'P' ? lv_color_make(0x3f, 0x78, 0xd6)
                        : lv_color_make(0xe0, 0x78, 0x2c);
    lv_area_t c;
    lv_draw_rect_dsc_t rect;
    lv_draw_line_dsc_t line;
    lv_area_t a;
    lv_area_t plate;
    lv_color_t glyph = lv_color_white();

    lv_obj_get_coords(obj, &c);
    lv_draw_rect_dsc_init(&rect);
    lv_draw_line_dsc_init(&line);
    rect.bg_opa = LV_OPA_COVER;
    line.color = glyph;
    line.width = 4;
    line.round_start = 1;
    line.round_end = 1;
    plate.x1 = c.x1 + (lv_obj_get_width(obj) - ICON_PLATE_SIZE) / 2 + 6;
    plate.y1 = c.y1 + ICON_PLATE_Y + 6;
    plate.x2 = plate.x1 + CELLPHONE_ICON_SIZE - 1;
    plate.y2 = plate.y1 + CELLPHONE_ICON_SIZE - 1;
    rect.bg_color = accent;
    rect.radius = 9;
    lv_draw_rect(layer, &rect, &plate);

    rect.bg_color = lv_color_mix(accent, lv_color_white(), LV_OPA_20);
    rect.radius = 8;
    a.x1 = plate.x1 + 2;
    a.y1 = plate.y1 + 2;
    a.x2 = plate.x2 - 2;
    a.y2 = plate.y1 + 8;
    lv_draw_rect(layer, &rect, &a);

    if(kind == 'S') {
        lv_point_precise_t pts[] = {
            { plate.x1 + 8,  plate.y1 + 24 }, { plate.x1 + 15, plate.y1 + 24 },
            { plate.x1 + 15, plate.y1 + 17 }, { plate.x1 + 23, plate.y1 + 17 },
            { plate.x1 + 23, plate.y1 + 27 }, { plate.x1 + 30, plate.y1 + 27 }
        };
        line.points = pts;
        line.point_cnt = (int32_t)(sizeof(pts) / sizeof(pts[0]));
        lv_draw_line(layer, &line);

        rect.bg_color = glyph;
        rect.radius = LV_RADIUS_CIRCLE;
        a.x1 = plate.x1 + 27;
        a.y1 = plate.y1 + 20;
        a.x2 = plate.x1 + 34;
        a.y2 = plate.y1 + 27;
        lv_draw_rect(layer, &rect, &a);

        rect.bg_color = accent;
        a.x1 = plate.x1 + 30;
        a.y1 = plate.y1 + 22;
        a.x2 = plate.x1 + 31;
        a.y2 = plate.y1 + 23;
        lv_draw_rect(layer, &rect, &a);

        rect.bg_color = lv_color_hex(0xe53935);
        a.x1 = plate.x1 + 6;
        a.y1 = plate.y1 + 10;
        a.x2 = plate.x1 + 11;
        a.y2 = plate.y1 + 15;
        lv_draw_rect(layer, &rect, &a);
    }
    else if(kind == 'P') {
        rect.bg_color = glyph;
        rect.radius = 3;
        a.x1 = plate.x1 + 6;
        a.y1 = plate.y1 + 11;
        a.x2 = plate.x1 + 10;
        a.y2 = plate.y1 + 30;
        lv_draw_rect(layer, &rect, &a);
        a.x1 = plate.x2 - 9;
        a.y1 = plate.y1 + 11;
        a.x2 = plate.x2 - 5;
        a.y2 = plate.y1 + 30;
        lv_draw_rect(layer, &rect, &a);

        rect.bg_color = lv_color_hex(0x4dd0e1);
        rect.radius = LV_RADIUS_CIRCLE;
        a.x1 = plate.x1 + 17;
        a.y1 = plate.y1 + 16;
        a.x2 = plate.x1 + 23;
        a.y2 = plate.y1 + 22;
        lv_draw_rect(layer, &rect, &a);

        rect.bg_color = lv_color_mix(glyph, accent, LV_OPA_40);
        rect.radius = 1;
        a.x1 = plate.x1 + 19;
        a.y1 = plate.y1 + 8;
        a.x2 = plate.x1 + 20;
        a.y2 = plate.y1 + 13;
        lv_draw_rect(layer, &rect, &a);
        a.y1 = plate.y1 + 25;
        a.y2 = plate.y1 + 30;
        lv_draw_rect(layer, &rect, &a);
    }
    else {
        static const struct {
            int16_t x;
            int16_t y;
            uint8_t color_id;
        } blocks[] = {
            { 8, 9,  0 }, { 14, 9, 0 }, { 20, 9, 0 }, { 26, 9, 0 },
            { 14, 16, 1 }, { 20, 16, 1 }, { 14, 23, 2 }, { 20, 23, 2 },
        };

        rect.radius = 2;
        for(uint32_t i = 0; i < sizeof(blocks) / sizeof(blocks[0]); i++) {
            if(blocks[i].color_id == 0) rect.bg_color = glyph;
            else if(blocks[i].color_id == 1) rect.bg_color = lv_color_make(0xff, 0xd5, 0x4f);
            else rect.bg_color = lv_color_make(0x81, 0xd4, 0xfa);
            a.x1 = plate.x1 + blocks[i].x;
            a.y1 = plate.y1 + blocks[i].y + 2;
            a.x2 = a.x1 + 5;
            a.y2 = a.y1 + 5;
            lv_draw_rect(layer, &rect, &a);
        }
    }
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
    s_picked_icon = pressed;
    cellphone_anim_pickup(pressed);
}

static void indicator_set_active(uint32_t active_idx, bool animated)
{
    for(uint32_t d = 0; d < s_page_count; d++) {
        lv_obj_t * dot = s_dots[d];
        if(!dot) continue;

        bool active = d == active_idx;
        int32_t target_w = active ? INDICATOR_ACTIVE_W : INDICATOR_DOT_SIZE;
        lv_color_t color = active ? CELLPHONE_COLOR_PRIMARY : CELLPHONE_COLOR_INDICATOR;
        lv_opa_t opa = active ? LV_OPA_COVER : LV_OPA_70;

        lv_obj_set_style_bg_color(dot, color, 0);
        lv_obj_set_style_bg_opa(dot, opa, 0);

        if(animated) {
            cellphone_anim_run(dot, cellphone_anim_set_width_cb,
                               lv_obj_get_width(dot), target_w,
                               CELLPHONE_MOTION_QUICK.enter_ms,
                               CELLPHONE_MOTION_QUICK.path_cb);
        }
        else {
            lv_obj_set_width(dot, target_w);
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
    indicator_set_active((uint32_t)col, true);
}

#endif /* LV_USE_DEMO_CELLPHONE */
