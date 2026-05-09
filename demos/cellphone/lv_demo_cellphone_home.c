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
/* Modern squircle look: ~27% of plate side; iOS uses ~22%, Material You
 * uses ~30%. 14 / 52 = 0.269 sits between them. */
#define ICON_PLATE_RADIUS    14
/* Gradient mix toward black at the bottom of the plate -- 14% is enough
 * to read as depth without looking 2010s skeuomorphic. */
#define ICON_PLATE_GRAD_MIX  LV_OPA_20

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void icon_event_cb(lv_event_t * e);
static void page_changed_cb(lv_event_t * e);
static lv_obj_t * create_icon(lv_obj_t * parent, const cellphone_app_entry_t * app);
static void icon_plate_draw_cb(lv_event_t * e);
static void indicator_set_active(uint32_t active_idx, bool animated);
static void glyph_draw_block(lv_layer_t * layer, lv_draw_rect_dsc_t * dsc,
                             int32_t x1, int32_t y1, int32_t x2, int32_t y2,
                             lv_color_t color, int32_t radius);

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
                                              &apps[app_idx]);
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

static lv_obj_t * create_icon(lv_obj_t * parent, const cellphone_app_entry_t * app)
{
    lv_obj_t * cont = cellphone_obj_transparent(parent);
    lv_obj_set_size(cont, ICON_LABEL_W + 12, CELLPHONE_ICON_SIZE + CELLPHONE_ICON_LABEL_H + 18);
    lv_obj_add_flag(cont, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_OVERFLOW_VISIBLE);

    /* All icons render through the procedural plate -- one
     * LV_EVENT_DRAW_MAIN per icon, zero child objects, consistent
     * modern look across the grid. */
    lv_obj_add_event_cb(cont, icon_plate_draw_cb, LV_EVENT_DRAW_MAIN, (void *)app);

    /* Label below icon */
    lv_obj_t * lbl = cellphone_label(cont, app->name,
                                     CELLPHONE_FONT_SM, CELLPHONE_COLOR_TEXT);
    lv_obj_set_width(lbl, ICON_LABEL_W);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_CLIP);
    lv_obj_align(lbl, LV_ALIGN_TOP_MID, 0, ICON_LABEL_Y);

    /* One callback handles both click and long-press, avoiding a second
     * event descriptor per icon. */
    lv_obj_add_event_cb(cont, icon_event_cb, LV_EVENT_ALL, (void *)app->create);

    return cont;
}

static void icon_plate_draw_cb(lv_event_t * e)
{
    lv_obj_t * obj = lv_event_get_target(e);
    lv_layer_t * layer = lv_event_get_layer(e);
    const cellphone_app_entry_t * app = lv_event_get_user_data(e);
    lv_draw_rect_dsc_t rect_dsc;
    lv_color_t accent;
    lv_area_t plate;
    lv_area_t coords;

    if(app == NULL) return;

    accent = lv_color_hex(app->icon_color_hex);
    lv_obj_get_coords(obj, &coords);

    plate.x1 = coords.x1 + (lv_obj_get_width(obj) - ICON_PLATE_SIZE) / 2;
    plate.y1 = coords.y1 + ICON_PLATE_Y;
    plate.x2 = plate.x1 + ICON_PLATE_SIZE - 1;
    plate.y2 = plate.y1 + ICON_PLATE_SIZE - 1;

    /* Modern squircle plate: solid accent at the top transitioning to a
     * slightly darkened accent at the bottom, no gloss strip. The
     * gradient is a single 2-stop linear so it composes with the MCU
     * profile's reduced gradient-stop budget. */
    lv_draw_rect_dsc_init(&rect_dsc);
    rect_dsc.bg_color = accent;
    rect_dsc.bg_grad.dir = LV_GRAD_DIR_VER;
    rect_dsc.bg_grad.stops_count = 2;
    rect_dsc.bg_grad.stops[0].color = accent;
    rect_dsc.bg_grad.stops[0].opa = LV_OPA_COVER;
    rect_dsc.bg_grad.stops[0].frac = 0;
    rect_dsc.bg_grad.stops[1].color = lv_color_mix(lv_color_black(), accent,
                                                   ICON_PLATE_GRAD_MIX);
    rect_dsc.bg_grad.stops[1].opa = LV_OPA_COVER;
    rect_dsc.bg_grad.stops[1].frac = 255;
    rect_dsc.bg_opa = LV_OPA_COVER;
    rect_dsc.radius = ICON_PLATE_RADIUS;
    lv_draw_rect(layer, &rect_dsc, &plate);

    /* Glyph: either a bundled FA5 symbol drawn as text, or a custom
     * procedural silhouette. Exactly one of icon_text / glyph_draw is
     * set per registry entry. */
    if(app->icon_text) {
        lv_draw_label_dsc_t label_dsc;
        lv_point_t text_size;
        lv_area_t glyph_area;

        lv_draw_label_dsc_init(&label_dsc);
        label_dsc.color = lv_color_white();
        label_dsc.opa = LV_OPA_COVER;
        label_dsc.font = CELLPHONE_FONT_LARGE;
        label_dsc.text = app->icon_text;

        lv_text_get_size(&text_size, app->icon_text, label_dsc.font,
                         0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
        glyph_area.x1 = plate.x1 + (lv_area_get_width(&plate) - text_size.x) / 2;
        glyph_area.y1 = plate.y1 + (lv_area_get_height(&plate) - text_size.y) / 2;
        glyph_area.x2 = glyph_area.x1 + text_size.x - 1;
        glyph_area.y2 = glyph_area.y1 + text_size.y - 1;
        lv_draw_label(layer, &label_dsc, &glyph_area);
    }
    else if(app->glyph_draw) {
        app->glyph_draw(layer, &plate, accent);
    }
}

/* Stamp a small color block inside the plate. The plate-glyph drawers
 * call this dozens of times each with only color/coords/radius
 * varying; centralizing keeps the per-drawer code dense. */
static void glyph_draw_block(lv_layer_t * layer, lv_draw_rect_dsc_t * dsc,
                             int32_t x1, int32_t y1, int32_t x2, int32_t y2,
                             lv_color_t color, int32_t radius)
{
    lv_area_t a = { .x1 = x1, .y1 = y1, .x2 = x2, .y2 = y2 };
    dsc->bg_color = color;
    dsc->radius = radius;
    lv_draw_rect(layer, dsc, &a);
}

/* All glyph drawers share the same convention:
 *   - cx, cy = plate center; px1/py1 = plate top-left corner.
 *   - Glyph silhouette stays inside a 32 px box centered on the plate.
 *   - White is the primary stroke; `accent` is mixed in only for inner
 *     shading so the icon always reads on its own colored plate. */

void cellphone_glyph_messages(lv_layer_t * layer, const lv_area_t * plate, lv_color_t accent)
{
    int32_t cx = (plate->x1 + plate->x2) / 2;
    int32_t cy = (plate->y1 + plate->y2) / 2;
    lv_color_t white = lv_color_white();
    lv_color_t shade = lv_color_mix(lv_color_black(), accent, LV_OPA_60);

    lv_draw_rect_dsc_t r;
    lv_draw_rect_dsc_init(&r);
    r.bg_opa = LV_OPA_COVER;

    /* Speech bubble body with a tail in the bottom-left corner. */
    glyph_draw_block(layer, &r, cx - 14, cy - 11, cx + 14, cy + 7, white, 4);
    /* Tail: small triangle approximation via two stacked rounded rects. */
    glyph_draw_block(layer, &r, cx - 12, cy + 5, cx - 6, cy + 11, white, 2);
    glyph_draw_block(layer, &r, cx - 11, cy + 8, cx - 8, cy + 12, white, 1);

    /* Three reading dots so the bubble reads as a chat thread, not a
     * blank card. Drawn in shade for legibility on the white body. */
    glyph_draw_block(layer, &r, cx - 7, cy - 3, cx - 4, cy, shade, LV_RADIUS_CIRCLE);
    glyph_draw_block(layer, &r, cx - 1, cy - 3, cx + 2, cy, shade, LV_RADIUS_CIRCLE);
    glyph_draw_block(layer, &r, cx + 5, cy - 3, cx + 8, cy, shade, LV_RADIUS_CIRCLE);
}

void cellphone_glyph_calculator(lv_layer_t * layer, const lv_area_t * plate, lv_color_t accent)
{
    int32_t cx = (plate->x1 + plate->x2) / 2;
    int32_t cy = (plate->y1 + plate->y2) / 2;
    lv_color_t white = lv_color_white();
    lv_color_t key = lv_color_mix(lv_color_black(), accent, LV_OPA_70);

    lv_draw_rect_dsc_t r;
    lv_draw_rect_dsc_init(&r);
    r.bg_opa = LV_OPA_COVER;

    /* Calculator body. */
    glyph_draw_block(layer, &r, cx - 13, cy - 14, cx + 13, cy + 14, white, 4);
    /* Display strip across the top. */
    glyph_draw_block(layer, &r, cx - 10, cy - 11, cx + 10, cy - 6, key, 1);
    /* 3 x 3 keypad grid (small dark squares spaced 6 px apart). */
    for(int row = 0; row < 3; row++) {
        for(int col = 0; col < 3; col++) {
            int32_t x = cx - 9 + col * 7;
            int32_t y = cy - 2 + row * 6;
            glyph_draw_block(layer, &r, x, y, x + 4, y + 3, key, 1);
        }
    }
}

void cellphone_glyph_camera(lv_layer_t * layer, const lv_area_t * plate, lv_color_t accent)
{
    int32_t cx = (plate->x1 + plate->x2) / 2;
    int32_t cy = (plate->y1 + plate->y2) / 2;
    lv_color_t white = lv_color_white();
    lv_color_t lens_dk = lv_color_mix(lv_color_black(), accent, LV_OPA_70);
    lv_color_t glass = lv_color_mix(white, accent, LV_OPA_60);

    lv_draw_rect_dsc_t r;
    lv_draw_rect_dsc_init(&r);
    r.bg_opa = LV_OPA_COVER;

    /* Viewfinder bump (sits on top of the body). */
    glyph_draw_block(layer, &r, cx - 5, cy - 12, cx + 5, cy - 8, white, 2);
    /* Body. */
    glyph_draw_block(layer, &r, cx - 14, cy - 8, cx + 14, cy + 10, white, 4);
    /* Lens outer ring + inner glass + flash dot. */
    glyph_draw_block(layer, &r, cx - 7, cy - 4, cx + 7, cy + 8, lens_dk, LV_RADIUS_CIRCLE);
    glyph_draw_block(layer, &r, cx - 4, cy - 1, cx + 4, cy + 5, glass, LV_RADIUS_CIRCLE);
    glyph_draw_block(layer, &r, cx + 9, cy - 6, cx + 12, cy - 3, lens_dk, LV_RADIUS_CIRCLE);
}

void cellphone_glyph_snake(lv_layer_t * layer, const lv_area_t * plate, lv_color_t accent)
{
    int32_t cx = (plate->x1 + plate->x2) / 2;
    int32_t cy = (plate->y1 + plate->y2) / 2;
    lv_color_t white = lv_color_white();

    /* Snake body: 5 stacked round segments forming a zigzag, head at
     * the right. Each segment is a 6 px circle. */
    static const struct {
        int8_t dx;
        int8_t dy;
    } seg[] = {
        { -12, 4 }, { -6, 4 }, { -6, -2 }, { 0, -2 }, { 0, 4 },
        { 6, 4 }, { 12, 4 }, { 12, -2 },
    };

    lv_draw_rect_dsc_t r;
    lv_draw_rect_dsc_init(&r);
    r.bg_opa = LV_OPA_COVER;

    for(uint32_t i = 0; i < sizeof(seg) / sizeof(seg[0]); i++) {
        int32_t x = cx + seg[i].dx - 2;
        int32_t y = cy + seg[i].dy - 2;
        glyph_draw_block(layer, &r, x, y, x + 4, y + 4, white, LV_RADIUS_CIRCLE);
    }
    /* Apple: small accented dot above the head. */
    glyph_draw_block(layer, &r, cx + 10, cy - 11, cx + 14, cy - 7,
                     lv_color_hex(0xef4444), LV_RADIUS_CIRCLE);
}

void cellphone_glyph_pong(lv_layer_t * layer, const lv_area_t * plate, lv_color_t accent)
{
    int32_t cx = (plate->x1 + plate->x2) / 2;
    int32_t cy = (plate->y1 + plate->y2) / 2;
    lv_color_t white = lv_color_white();
    lv_color_t net = lv_color_mix(white, accent, LV_OPA_50);

    lv_draw_rect_dsc_t r;
    lv_draw_rect_dsc_init(&r);
    r.bg_opa = LV_OPA_COVER;

    /* Two paddles flanking a centered ball. */
    glyph_draw_block(layer, &r, cx - 14, cy - 9, cx - 11, cy + 9, white, 2);
    glyph_draw_block(layer, &r, cx + 11, cy - 9, cx + 14, cy + 9, white, 2);
    glyph_draw_block(layer, &r, cx - 2, cy - 2, cx + 2, cy + 2, white, LV_RADIUS_CIRCLE);
    /* Center net stipple: 3 dashes top-to-bottom. */
    for(int i = -10; i <= 10; i += 8) {
        glyph_draw_block(layer, &r, cx - 1, cy + i, cx + 1, cy + i + 3, net, 1);
    }
}

void cellphone_glyph_tetris(lv_layer_t * layer, const lv_area_t * plate, lv_color_t accent)
{
    int32_t cx = (plate->x1 + plate->x2) / 2;
    int32_t cy = (plate->y1 + plate->y2) / 2;
    lv_color_t white = lv_color_white();
    /* Two accent tints so the falling-piece silhouette has visual
     * weight without leaving the icon's color identity. The warm tint
     * sits ~50% white so it stays readable on the bottom of the plate
     * (which the gradient darkens by ICON_PLATE_GRAD_MIX); a softer
     * mix vanishes on red/pink palettes. */
    lv_color_t warm = lv_color_mix(white, accent, LV_OPA_50);
    lv_color_t cool = lv_color_mix(white, accent, LV_OPA_70);

    lv_draw_rect_dsc_t r;
    lv_draw_rect_dsc_init(&r);
    r.bg_opa = LV_OPA_COVER;

    /* Classic L-tetromino + a 2x1 floating piece. Each block is 6x6. */
    static const struct {
        int8_t dx;
        int8_t dy;
        uint8_t tone; /* 0=white, 1=warm, 2=cool */
    } blocks[] = {
        { -10, -2, 0 }, { -4, -2, 0 }, { 2, -2, 0 }, { 8, -2, 0 },
        { -4, 4, 1 }, { 2, 4, 1 },
        { -4, 10, 2 }, { 2, 10, 2 },
    };

    const lv_color_t palette[3] = { white, warm, cool };

    for(uint32_t i = 0; i < sizeof(blocks) / sizeof(blocks[0]); i++) {
        int32_t x = cx + blocks[i].dx;
        int32_t y = cy + blocks[i].dy;
        glyph_draw_block(layer, &r, x, y, x + 5, y + 5,
                         palette[blocks[i].tone], 1);
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
