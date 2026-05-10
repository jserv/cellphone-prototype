/**
 * @file lv_demo_cellphone_photo.c
 *
 * Photo viewer: 3x2 JPEG thumbnail grid + fullscreen tileview browser.
 */

#include "lv_demo_cellphone_photo.h"
#include "lv_demo_cellphone_anim.h"
#include <stddef.h>

#if LV_USE_DEMO_CELLPHONE

/*********************
 *      DEFINES
 *********************/
#define PHOTO_COUNT     6
#define GRID_COLS       3
#define GRID_ROWS       2
#define THUMB_GAP       4
#define THUMB_LABEL_H   18
#define THUMB_RADIUS    6
#define VIEWER_RADIUS   10
/* Square cells: (content_w - outer_pad*2 - inter_col_gap*2) / cols. */
#define THUMB_CELL_PX   ((CELLPHONE_CONTENT_W - THUMB_GAP * 4) / GRID_COLS)
#define VIEWER_TITLE_H  28
#define VIEWER_COUNTER_H 24
#define PHOTO_IDX_NONE  ((uint32_t) -1)
#define PHOTO_ASSET(title_, stem_) { \
        title_, \
        CELLPHONE_PHOTO_PATH("thumbs/" stem_ "_thumb.jpg"), \
        CELLPHONE_PHOTO_PATH("view/" stem_ "_view.jpg") \
    }

#ifndef CELLPHONE_PHOTOS_DIR
    #define CELLPHONE_PHOTOS_DIR "demos/cellphone/assets/photos"
#endif

#ifndef LV_DEMO_CELLPHONE_PHOTOS_SOURCE
    #define LV_DEMO_CELLPHONE_PHOTOS_SOURCE CELLPHONE_PHOTO_SOURCE_AUTO
#endif

#define CELLPHONE_PHOTOS_FS_PREFIX "P:"
#define CELLPHONE_PHOTO_PATH(name) CELLPHONE_PHOTOS_FS_PREFIX CELLPHONE_PHOTOS_DIR "/" name

#if LV_USE_TJPGD && LV_USE_FS_STDIO
    #define CELLPHONE_PHOTOS_REAL_SUPPORT 1
#else
    #define CELLPHONE_PHOTOS_REAL_SUPPORT 0
#endif

typedef struct {
    const char * title;
    const char * thumb_path;
    const char * view_path;
} cellphone_photo_asset_t;

/**********************
 *  STATIC VARIABLES
 **********************/
static uint32_t s_selected_photo_idx;
static uint32_t s_synced_photo_idx = PHOTO_IDX_NONE;
/* Runtime asset probe cache: 0 = unknown, 1 = present, 2 = missing.
 * Probes once on first source-mode query so a missing assets directory
 * silently falls back to dummy art without re-stating the FS each open. */
static uint8_t s_assets_probe;

/**********************
 *  STATIC PROTOTYPES
 **********************/
static lv_obj_t * photo_viewer_create(lv_obj_t * parent);
static void thumb_click_cb(lv_event_t * e);
static void viewer_tap_cb(lv_event_t * e);
static void viewer_tile_changed_cb(lv_event_t * e);
static void viewer_visibility_cb(lv_event_t * e);
static void viewer_load_timer_cb(lv_timer_t * timer);
static void viewer_cancel_load_timer(void);
static void viewer_enter_deferred_mode(void);
static void viewer_sync_loaded_images(void);
static void viewer_schedule_loaded_image_sync(void);
static void viewer_set_active_title(uint32_t photo_idx);
static void update_counter_label(uint32_t photo_idx);
static void viewer_delete_cb(lv_event_t * e);
static void init_photo_surface(lv_obj_t * obj, bool clickable,
                               lv_coord_t radius, lv_event_cb_t click_cb, void * user_data);
static lv_obj_t * create_photo_card(lv_obj_t * parent, const cellphone_photo_asset_t * asset,
                                    const char * image_path, bool clickable, bool show_caption,
                                    lv_coord_t radius, lv_coord_t label_h,
                                    lv_event_cb_t click_cb, void * user_data);
static void create_dummy_art(lv_obj_t * parent, const cellphone_photo_asset_t * asset);
static lv_color_t photo_color(uint32_t idx);
static uint32_t photo_asset_index(const cellphone_photo_asset_t * asset);
static void create_unavailable_notice(lv_obj_t * parent, const char * title, const char * body);

static const cellphone_photo_asset_t s_photo_assets[PHOTO_COUNT] = {
    PHOTO_ASSET("Portrait", "bob_kerrey_portrait"),
    PHOTO_ASSET("Landscape", "landscape_1677"),
    PHOTO_ASSET("Constitution", "us_constitution"),
    PHOTO_ASSET("Solar Loops", "solar_magnetic_loops"),
    PHOTO_ASSET("Apollo 16", "apollo16_jump"),
    PHOTO_ASSET("Cupola", "iss_window"),
};

static lv_obj_t * s_viewer_root;
static lv_obj_t * s_viewer_title;
static lv_obj_t * s_viewer_active_title_label;
static lv_obj_t * s_viewer_counter;
static lv_obj_t * s_viewer_counter_label;
static lv_obj_t * s_viewer_images[PHOTO_COUNT];
static lv_timer_t * s_viewer_load_timer;
static bool s_fullscreen;
static bool s_viewer_highres_ready;

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

lv_obj_t * cellphone_photo_create(lv_obj_t * parent)
{
    lv_obj_set_size(parent, CELLPHONE_CONTENT_W, CELLPHONE_CONTENT_H);
    cellphone_obj_paint_fill(parent, CELLPHONE_COLOR_BG);
    lv_obj_set_style_pad_all(parent, THUMB_GAP, 0);

    lv_obj_t * title = cellphone_label(parent, "Photos",
                                       CELLPHONE_FONT_HEADING, CELLPHONE_COLOR_TEXT);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 4, 0);

    if(!cellphone_photo_is_available()) {
        const char * body = cellphone_photo_real_jpeg_supported()
                            ? "Photo assets were not found at runtime under "
                            "CELLPHONE_PHOTOS_DIR; rebuild with "
                            "-DLV_DEMO_CELLPHONE_PHOTOS_DIR=... or set "
                            "LV_DEMO_CELLPHONE_PHOTOS_SOURCE=2 (dummy)."
                            : "This build forces real JPEG photos, but "
                            "LV_USE_TJPGD or LV_USE_FS_STDIO is disabled.";
        create_unavailable_notice(parent, "Real JPEG handler unavailable", body);
        return parent;
    }

    lv_obj_t * grid = cellphone_obj_bare(parent);
    lv_obj_set_size(grid, THUMB_CELL_PX * GRID_COLS + THUMB_GAP * (GRID_COLS - 1),
                    THUMB_CELL_PX * GRID_ROWS + THUMB_GAP * (GRID_ROWS - 1));
    lv_obj_align(grid, LV_ALIGN_TOP_MID, 0, 22);
    lv_obj_set_layout(grid, LV_LAYOUT_GRID);

    static const int32_t col_dsc[] = {
        THUMB_CELL_PX, THUMB_CELL_PX, THUMB_CELL_PX, LV_GRID_TEMPLATE_LAST
    };
    static const int32_t row_dsc[] = {
        THUMB_CELL_PX, THUMB_CELL_PX, LV_GRID_TEMPLATE_LAST
    };

    lv_obj_set_grid_dsc_array(grid, col_dsc, row_dsc);
    lv_obj_set_style_pad_column(grid, THUMB_GAP, 0);
    lv_obj_set_style_pad_row(grid, THUMB_GAP, 0);

    uint32_t i;
    for(i = 0; i < PHOTO_COUNT; i++) {
        uint32_t col = i % GRID_COLS;
        uint32_t row = i / GRID_COLS;

        lv_obj_t * thumb = cellphone_obj_fill(grid, CELLPHONE_COLOR_CARD);
        lv_obj_set_style_radius(thumb, 6, 0);
        lv_obj_set_style_clip_corner(thumb, true, 0);
        lv_obj_set_style_border_width(thumb, 1, 0);
        lv_obj_set_style_border_color(thumb, CELLPHONE_COLOR_NAVBAR, 0);
        lv_obj_set_grid_cell(thumb,
                             LV_GRID_ALIGN_STRETCH, col, 1,
                             LV_GRID_ALIGN_STRETCH, row, 1);
        create_photo_card(thumb, &s_photo_assets[i], s_photo_assets[i].thumb_path,
                          true, true, THUMB_RADIUS, THUMB_LABEL_H,
                          thumb_click_cb, (void *)(uintptr_t)i);
    }

    return parent;
}

uint32_t cellphone_photo_source_config(void)
{
    return LV_DEMO_CELLPHONE_PHOTOS_SOURCE;
}

bool cellphone_photo_real_jpeg_supported(void)
{
    return CELLPHONE_PHOTOS_REAL_SUPPORT;
}

static bool runtime_assets_present(void)
{
    if(s_assets_probe != 0) return s_assets_probe == 1;
    if(!CELLPHONE_PHOTOS_REAL_SUPPORT) {
        s_assets_probe = 2;
        return false;
    }
    /* One canonical asset suffices: a missing FS letter, missing dir, or
     * missing file all surface as the same FS_RES error. */
    uint32_t size = 0;
    bool ok = lv_fs_path_get_size(s_photo_assets[0].thumb_path, &size) == LV_FS_RES_OK
              && size > 0;
    s_assets_probe = ok ? 1 : 2;
    return ok;
}

bool cellphone_photo_uses_real_jpeg(void)
{
    if(cellphone_photo_source_config() == CELLPHONE_PHOTO_SOURCE_DUMMY) return false;
    if(!cellphone_photo_real_jpeg_supported()) return false;
    return runtime_assets_present();
}

bool cellphone_photo_is_available(void)
{
    /* The dummy path has no external dependencies, so the screen is
     * always reachable; the unavailable notice only fires when the user
     * forced REAL_JPEG against a build that can't honor it. */
    if(cellphone_photo_source_config() == CELLPHONE_PHOTO_SOURCE_REAL_JPEG) {
        return cellphone_photo_real_jpeg_supported() && runtime_assets_present();
    }

    return true;
}

const char * cellphone_photo_source_name(void)
{
    if(cellphone_photo_uses_real_jpeg()) return "real-jpeg";
    if(cellphone_photo_source_config() != CELLPHONE_PHOTO_SOURCE_REAL_JPEG &&
       CELLPHONE_PHOTOS_REAL_SUPPORT && !runtime_assets_present()) {
        return "dummy-assets-missing";
    }
    if(cellphone_photo_is_available()) return "dummy";
    return "unavailable-real-jpeg";
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void thumb_click_cb(lv_event_t * e)
{
    s_selected_photo_idx = (uint32_t)(uintptr_t)lv_event_get_user_data(e);
    cellphone_screen_push(photo_viewer_create);
}

static lv_obj_t * photo_viewer_create(lv_obj_t * parent)
{
    lv_obj_t * screen = lv_obj_get_parent(parent);

    viewer_cancel_load_timer();
    s_fullscreen = false;
    s_viewer_highres_ready = false;
    s_synced_photo_idx = PHOTO_IDX_NONE;
    s_viewer_root = parent;

    lv_obj_set_size(parent, CELLPHONE_CONTENT_W, CELLPHONE_CONTENT_H);
    cellphone_obj_paint_fill(parent, lv_color_black());
    lv_obj_set_style_pad_all(parent, 0, 0);

    s_viewer_title = cellphone_obj_bare(parent);
    lv_obj_set_size(s_viewer_title, CELLPHONE_CONTENT_W, VIEWER_TITLE_H);
    lv_obj_set_pos(s_viewer_title, 0, 0);
    lv_obj_set_style_bg_color(s_viewer_title, lv_color_hex(0x1a1a1a), 0);
    lv_obj_set_style_bg_opa(s_viewer_title, LV_OPA_80, 0);
    lv_obj_clear_flag(s_viewer_title, LV_OBJ_FLAG_SCROLLABLE);
    /* Keep fullscreen toggling reachable even when the tile cards are
     * underneath the title bar. */
    lv_obj_add_flag(s_viewer_title, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_viewer_title, viewer_tap_cb, LV_EVENT_CLICKED, parent);

    lv_obj_t * title_lbl = cellphone_label(s_viewer_title, "Photos",
                                           CELLPHONE_FONT_SM, lv_color_white());
    lv_obj_align(title_lbl, LV_ALIGN_LEFT_MID, 8, 0);

    /* The active photo title lives in chrome once, instead of being
     * duplicated on every tile. */
    s_viewer_active_title_label = cellphone_label(s_viewer_title, NULL,
                                                  CELLPHONE_FONT_SM, lv_color_white());
    lv_obj_align(s_viewer_active_title_label, LV_ALIGN_RIGHT_MID, -8, 0);

    /* Counter bar at bottom */
    s_viewer_counter = cellphone_obj_bare(parent);
    lv_obj_set_size(s_viewer_counter, CELLPHONE_CONTENT_W, VIEWER_COUNTER_H);
    lv_obj_align(s_viewer_counter, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(s_viewer_counter, lv_color_hex(0x1a1a1a), 0);
    lv_obj_set_style_bg_opa(s_viewer_counter, LV_OPA_80, 0);
    lv_obj_clear_flag(s_viewer_counter, LV_OBJ_FLAG_SCROLLABLE);

    s_viewer_counter_label = cellphone_label(s_viewer_counter, NULL,
                                             CELLPHONE_FONT_SM, lv_color_white());
    lv_obj_center(s_viewer_counter_label);

    if(s_selected_photo_idx >= PHOTO_COUNT) s_selected_photo_idx = 0;
    update_counter_label(s_selected_photo_idx);
    viewer_set_active_title(s_selected_photo_idx);

    lv_obj_t * tv = lv_tileview_create(parent);
    lv_obj_set_size(tv, CELLPHONE_CONTENT_W, CELLPHONE_CONTENT_H);
    cellphone_obj_paint_fill(tv, lv_color_black());

    lv_obj_move_background(tv);

    uint32_t i;
    for(i = 0; i < PHOTO_COUNT; i++) {
        lv_obj_t * tile = lv_tileview_add_tile(tv, i, 0, LV_DIR_HOR);
        /* Radius 0 on fullscreen tiles: a non-zero radius with clip_corner
         * makes lv_refr allocate two ARGB8888 corner-mask layers per draw
         * (~width * radius * 4 * 2 bytes), a ~19 KiB transient at 240x10. */
        uint32_t delta = i > s_selected_photo_idx ? i - s_selected_photo_idx : s_selected_photo_idx - i;
        const char * initial_path = (delta <= 1) ? s_photo_assets[i].thumb_path : NULL;
        s_viewer_images[i] = create_photo_card(tile, &s_photo_assets[i], initial_path,
                                               true, false, 0, 0,
                                               viewer_tap_cb, parent);
    }

    lv_obj_add_event_cb(tv, viewer_tile_changed_cb, LV_EVENT_VALUE_CHANGED, NULL);
    viewer_sync_loaded_images();
    viewer_schedule_loaded_image_sync();
    lv_tileview_set_tile_by_index(tv, s_selected_photo_idx, 0, LV_ANIM_OFF);

    lv_obj_add_event_cb(parent, viewer_delete_cb, LV_EVENT_DELETE, NULL);
    if(screen) {
        lv_obj_add_event_cb(screen, viewer_visibility_cb,
                            (lv_event_code_t)cellphone_screen_event_hidden(), NULL);
        lv_obj_add_event_cb(screen, viewer_visibility_cb,
                            (lv_event_code_t)cellphone_screen_event_shown(), NULL);
    }

    return parent;
}

static void viewer_delete_cb(lv_event_t * e)
{
    /* Screen-pop animations delay the delete by ~270 ms. If the user
     * re-pushes the viewer in that window, this fires for the previous
     * instance after the new instance has rebound the statics. Only the
     * still-current viewer should clear them. */
    if(lv_event_get_target(e) != s_viewer_root) return;
    s_viewer_root = NULL;
    s_viewer_title = NULL;
    s_viewer_active_title_label = NULL;
    s_viewer_counter = NULL;
    s_viewer_counter_label = NULL;
    viewer_cancel_load_timer();
    s_viewer_highres_ready = false;
    s_synced_photo_idx = PHOTO_IDX_NONE;
    lv_memzero(s_viewer_images, sizeof(s_viewer_images));
}

/* Toggle fullscreen by sliding the viewer chrome off-screen and back. */
static void viewer_tap_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    if(!s_viewer_title || !s_viewer_counter) return;

    int32_t title_target, counter_target;
    uint32_t dur;

    if(!s_fullscreen) {
        title_target = -VIEWER_TITLE_H;
        counter_target = CELLPHONE_CONTENT_H;
        dur = CELLPHONE_MOTION_STANDARD.enter_ms;
    }
    else {
        title_target = 0;
        counter_target = CELLPHONE_CONTENT_H - VIEWER_COUNTER_H;
        dur = CELLPHONE_MOTION_STANDARD.exit_ms;
    }

    cellphone_anim_run(s_viewer_title, cellphone_anim_set_y_cb,
                       lv_obj_get_y(s_viewer_title), title_target,
                       dur, CELLPHONE_MOTION_STANDARD.path_cb);
    cellphone_anim_run(s_viewer_counter, cellphone_anim_set_y_cb,
                       lv_obj_get_y(s_viewer_counter), counter_target,
                       dur, CELLPHONE_MOTION_STANDARD.path_cb);

    s_fullscreen = !s_fullscreen;
}

static void viewer_tile_changed_cb(lv_event_t * e)
{
    lv_obj_t * tv = lv_event_get_target(e);
    lv_obj_t * tile = lv_tileview_get_tile_active(tv);

    if(tile == NULL) return;

    int32_t tv_w = lv_obj_get_content_width(tv);
    if(tv_w <= 0) return;

    uint32_t new_idx = (uint32_t)(lv_obj_get_x(tile) / tv_w);
    if(new_idx == s_selected_photo_idx) return;

    s_selected_photo_idx = new_idx;
    viewer_enter_deferred_mode();
    update_counter_label(s_selected_photo_idx);
    viewer_set_active_title(s_selected_photo_idx);
}

static void viewer_visibility_cb(lv_event_t * e)
{
    /* Hidden: cancel the pending hires load -- decoding into off-screen
     * tiles is wasted work. Shown: re-arm the deferred load if the
     * initial defer window was interrupted, so the user isn't stuck on
     * thumbnails after a push/pop on top of the viewer. */
    lv_event_code_t code = lv_event_get_code(e);
    if(code == (lv_event_code_t)cellphone_screen_event_hidden()) {
        viewer_cancel_load_timer();
        return;
    }
    if(code == (lv_event_code_t)cellphone_screen_event_shown()) {
        if(!s_viewer_highres_ready) viewer_schedule_loaded_image_sync();
    }
}

static void viewer_load_timer_cb(lv_timer_t * timer)
{
    LV_UNUSED(timer);
    s_viewer_load_timer = NULL;
    s_viewer_highres_ready = true;
    s_synced_photo_idx = PHOTO_IDX_NONE;
    viewer_sync_loaded_images();
}

static void viewer_cancel_load_timer(void)
{
    if(s_viewer_load_timer) {
        lv_timer_delete(s_viewer_load_timer);
        s_viewer_load_timer = NULL;
    }
}

static void viewer_enter_deferred_mode(void)
{
    /* After the initial defer window has elapsed, a swipe must NOT drop
     * the visible tile from view_path back to thumb_path -- that thumb
     * pop is the visual regression we promised to avoid. Only the first
     * open (highres_ready==false) needs the timer-deferred load. */
    if(s_viewer_highres_ready) {
        s_synced_photo_idx = PHOTO_IDX_NONE;
        viewer_sync_loaded_images();
        return;
    }
    viewer_cancel_load_timer();
    s_synced_photo_idx = PHOTO_IDX_NONE;
    viewer_sync_loaded_images();
    viewer_schedule_loaded_image_sync();
}

static void viewer_sync_loaded_images(void)
{
    if(!cellphone_photo_uses_real_jpeg()) return;
    if(s_selected_photo_idx == s_synced_photo_idx) return;

    uint32_t i;
    for(i = 0; i < PHOTO_COUNT; i++) {
        if(s_viewer_images[i] == NULL) continue;

        uint32_t delta = i > s_selected_photo_idx ? i - s_selected_photo_idx : s_selected_photo_idx - i;
        const char * want;
        if(s_viewer_highres_ready) want = (delta <= 1) ? s_photo_assets[i].view_path : NULL;
        else want = (delta <= 1) ? s_photo_assets[i].thumb_path : NULL;
        /* lv_image_set_src strdups file paths, so pointer-equality against
         * lv_image_get_src never matches; compare by content instead. */
        const char * cur = (const char *)lv_image_get_src(s_viewer_images[i]);
        if(want == cur) continue;
        if(want && cur && lv_strcmp(cur, want) == 0) continue;
        lv_image_set_src(s_viewer_images[i], want);
    }

    s_synced_photo_idx = s_selected_photo_idx;
}

static void viewer_schedule_loaded_image_sync(void)
{
    if(!cellphone_photo_uses_real_jpeg()) return;
    if(s_viewer_highres_ready) return;
    if(s_viewer_load_timer) return;
    if(!cellphone_screen_transitions_enabled()) {
        s_viewer_highres_ready = true;
        s_synced_photo_idx = PHOTO_IDX_NONE;
        viewer_sync_loaded_images();
        return;
    }

    s_viewer_load_timer = lv_timer_create(viewer_load_timer_cb,
                                          CELLPHONE_MOTION_STANDARD.enter_ms + 20, NULL);
    if(s_viewer_load_timer) {
        lv_timer_set_repeat_count(s_viewer_load_timer, 1);
        return;
    }

    s_viewer_highres_ready = true;
    s_synced_photo_idx = PHOTO_IDX_NONE;
    viewer_sync_loaded_images();
}

static void viewer_set_active_title(uint32_t photo_idx)
{
    if(s_viewer_active_title_label == NULL) return;
    if(photo_idx >= PHOTO_COUNT) return;
    lv_label_set_text(s_viewer_active_title_label, s_photo_assets[photo_idx].title);
}

static void update_counter_label(uint32_t photo_idx)
{
    char cnt_buf[16];

    if(s_viewer_counter_label == NULL) return;

    lv_snprintf(cnt_buf, sizeof(cnt_buf), "%" LV_PRIu32 " / %d",
                photo_idx + 1, PHOTO_COUNT);
    lv_label_set_text(s_viewer_counter_label, cnt_buf);
}

static void init_photo_surface(lv_obj_t * obj, bool clickable,
                               lv_coord_t radius, lv_event_cb_t click_cb, void * user_data)
{
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_set_style_radius(obj, radius, 0);
    lv_obj_set_style_clip_corner(obj, true, 0);

    if(clickable) {
        lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
        if(click_cb) lv_obj_add_event_cb(obj, click_cb, LV_EVENT_CLICKED, user_data);
    }
}

static lv_obj_t * create_photo_card(lv_obj_t * parent, const cellphone_photo_asset_t * asset,
                                    const char * image_path, bool clickable, bool show_caption,
                                    lv_coord_t radius, lv_coord_t label_h,
                                    lv_event_cb_t click_cb, void * user_data)
{
    lv_obj_t * img = NULL;
    init_photo_surface(parent, clickable, radius, click_cb, user_data);

    if(cellphone_photo_uses_real_jpeg()) {
        img = lv_image_create(parent);
        if(img == NULL) return NULL;
        lv_obj_set_size(img, LV_PCT(100), LV_PCT(100));
        lv_image_set_inner_align(img, LV_IMAGE_ALIGN_COVER);
        if(image_path != NULL) lv_image_set_src(img, image_path);

        if(show_caption) {
            lv_obj_t * shade = cellphone_obj_bare(parent);
            lv_obj_set_size(shade, LV_PCT(100), label_h + 8);
            lv_obj_align(shade, LV_ALIGN_BOTTOM_MID, 0, 0);
            lv_obj_set_style_bg_color(shade, lv_color_black(), 0);
            lv_obj_set_style_bg_opa(shade, LV_OPA_50, 0);
            lv_obj_set_style_radius(shade, 0, 0);
            lv_obj_clear_flag(shade, LV_OBJ_FLAG_SCROLLABLE);

            lv_obj_t * label = cellphone_label(parent, asset->title,
                                               CELLPHONE_FONT_SM, lv_color_white());
            lv_obj_align(label, LV_ALIGN_BOTTOM_LEFT, 6, -4);
        }
    }
    else {
        create_dummy_art(parent, asset);
    }
    return img;
}

static void create_dummy_art(lv_obj_t * parent, const cellphone_photo_asset_t * asset)
{
    uint32_t idx = photo_asset_index(asset);

    cellphone_obj_paint_fill(parent, photo_color(idx));

    lv_obj_t * accent = cellphone_obj_bare(parent);
    lv_obj_set_size(accent, LV_PCT(100), 20);
    lv_obj_align(accent, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(accent, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(accent, LV_OPA_20, 0);
    lv_obj_clear_flag(accent, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * label = cellphone_label(parent, asset->title,
                                       CELLPHONE_FONT_HEADING, lv_color_white());
    lv_obj_center(label);
}

static lv_color_t photo_color(uint32_t idx)
{
    return lv_color_hsv_to_rgb(idx * 40, 70, 80);
}

static uint32_t photo_asset_index(const cellphone_photo_asset_t * asset)
{
    ptrdiff_t idx = asset - s_photo_assets;
    if(idx < 0 || idx >= PHOTO_COUNT) return 0;
    return (uint32_t)idx;
}

static void create_unavailable_notice(lv_obj_t * parent, const char * title, const char * body)
{
    lv_obj_t * card = cellphone_obj_fill(parent, CELLPHONE_COLOR_CARD);
    lv_obj_set_size(card, CELLPHONE_CONTENT_W - 24, 110);
    lv_obj_align(card, LV_ALIGN_CENTER, 0, 8);
    lv_obj_set_style_radius(card, 10, 0);
    lv_obj_set_style_pad_all(card, 10, 0);

    lv_obj_t * title_lbl = cellphone_label(card, title,
                                           CELLPHONE_FONT_HEADING, CELLPHONE_COLOR_TEXT);
    lv_obj_align(title_lbl, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t * body_lbl = cellphone_label(card, body,
                                          CELLPHONE_FONT_SM, CELLPHONE_COLOR_TEXT_SEC);
    lv_obj_set_width(body_lbl, lv_pct(100));
    lv_obj_align(body_lbl, LV_ALIGN_TOP_LEFT, 0, 28);
}

#endif /* LV_USE_DEMO_CELLPHONE */
