/**
 * @file lv_demo_cellphone.c
 *
 * Entry point, screen stack, and app registry for the Cell Phone Demo.
 */

/*********************
 *      INCLUDES
 *********************/
#include "lv_demo_cellphone.h"

#if LV_USE_DEMO_CELLPHONE

#include "lv_demo_cellphone_common.h"
#include <time.h>
#include "lv_demo_cellphone_anim.h"
#include "lv_demo_cellphone_home.h"
#include "lv_demo_cellphone_statusbar.h"
#include "lv_demo_cellphone_navbar.h"
#include "lv_demo_cellphone_lock.h"
#if defined(LV_DEMO_CELLPHONE_SKIN) && LV_DEMO_CELLPHONE_SKIN && LV_USE_SDL
    #include "lv_demo_cellphone_skin.h"
#endif
#include "lv_demo_cellphone_dialer.h"
#include "lv_demo_cellphone_contacts.h"
#include "lv_demo_cellphone_sms.h"
#include "lv_demo_cellphone_calc.h"
#include "lv_demo_cellphone_music.h"
#include "lv_demo_cellphone_photo.h"
#include "lv_demo_cellphone_data.h"
#include "lv_demo_cellphone_settings.h"
#include "lv_demo_cellphone_calllog.h"
#include "lv_demo_cellphone_camera.h"
#include "lv_demo_cellphone_game.h"

/*********************
 *      DEFINES
 *********************/
#define IDLE_LOCK_TIMEOUT_MS  30000
#define IDLE_POLL_PERIOD_MS   1000

/**********************
 *      TYPEDEFS
 **********************/
typedef struct {
    uint32_t screen_state;
    lv_coord_t scroll_x;
    lv_coord_t scroll_y;
    uint8_t game_state[CELLPHONE_GAME_REFRESH_STATE_SIZE];
    bool has_scroll;
    bool has_game_state;
} screen_refresh_state_t;

/**********************
 *  THEME + MOTION DATA
 **********************/

#if LV_USE_FONT_VEC
    extern const lv_font_vec_data_t lv_font_vec_cellphone_icons_data;
    extern const lv_font_vec_data_t lv_font_vec_Montserrat_data;

    static lv_font_t s_vec_font_12;
    static lv_font_t s_vec_font_14;
    static lv_font_t s_vec_font_16;
    static lv_font_t s_vec_font_22;
    static lv_font_t s_vec_font_32;
    /* No icon-fallback at FONT_SM (12 px): the demo never renders an
    * LV_SYMBOL_* glyph at this size (verified by source audit -- sm is
    * used for body-text, secondaries, list rows, dropdowns, all ASCII).
    * Dropping the instance saves an lv_font_t + lv_font_vec_dsc_t and
    * one L1-cache refcount slot. */
    static lv_font_t s_vec_icon_font_14;
    static lv_font_t s_vec_icon_font_16;
    static lv_font_t s_vec_icon_font_22;
    static lv_font_t s_vec_icon_font_32;
    static bool s_vec_fonts_ready = false;
#endif /* LV_USE_FONT_VEC */

/*
 * Theme table: with LV_USE_FONT_VEC the table is mutable so vec_fonts_init()
 * can patch in the real font pointers after the rasterizer is up. Without
 * that patching every CELLPHONE_FONT_xxx macro would resolve to
 * lv_font_vec_stub via the active theme. When LV_USE_FONT_VEC is off the
 * table is fully static and lives in rodata.
 */
#if LV_USE_FONT_VEC
static cellphone_theme_t s_themes[] = {
#else
static const cellphone_theme_t s_themes[] = {
#endif
    /* 0: Retro Olive -- feature-phone sage/olive palette */
    {
        .primary        = { .blue = 0x5e, .green = 0x9b, .red = 0x7a },  /* 0x7A9B5E sage */
        .primary_dk     = { .blue = 0x45, .green = 0x7a, .red = 0x5d },  /* 0x5D7A45 */
        .statusbar      = { .blue = 0xb5, .green = 0xcd, .red = 0xc5 },  /* 0xC5CDB5 light olive */
        .statusbar_grad = { .blue = 0x9c, .green = 0xb0, .red = 0xa8 },  /* 0xA8B09C darker companion stop */
        .navbar         = { .blue = 0x9c, .green = 0xb0, .red = 0xa8 },  /* 0xA8B09C medium olive */
        .navbar_grad    = { .blue = 0xb5, .green = 0xcd, .red = 0xc5 },  /* 0xC5CDB5 lighter companion stop */
        .bg             = { .blue = 0xba, .green = 0xd7, .red = 0xcf },  /* 0xCFD7BA olive sage; saturated enough for clear card contrast */
        .bg_grad        = { .blue = 0xb0, .green = 0xcd, .red = 0xc5 },  /* 0xC5CDB0 subtler companion stop, low contrast vs base */
        .card           = { .blue = 0xe8, .green = 0xf1, .red = 0xef },  /* 0xEFF1E8 cream */
        .text           = { .blue = 0x22, .green = 0x22, .red = 0x22 },  /* 0x222222 */
        .text_sec       = { .blue = 0x53, .green = 0x62, .red = 0x5d },  /* 0x5D6253 */
        .call_green     = { .blue = 0x5e, .green = 0x9b, .red = 0x7a },  /* 0x7A9B5E */
        .call_red       = { .blue = 0x4b, .green = 0x4b, .red = 0xbd },  /* 0xBD4B4B end-call button bg, ~4.9:1 with white text */
        .missed_red     = { .blue = 0x40, .green = 0x40, .red = 0xb8 },  /* 0xB84040 */
        .sms_sent       = { .blue = 0xc0, .green = 0xe8, .red = 0xdc },  /* 0xDCE8C0 leaf */
        .sms_recv       = { .blue = 0xe8, .green = 0xf1, .red = 0xef },  /* 0xEFF1E8 cream */
        .indicator      = { .blue = 0x9c, .green = 0xb0, .red = 0xa8 },  /* 0xA8B09C */
#if LV_USE_FONT_VEC
        /* Compile-time placeholders; overridden at runtime by cellphone_theme_active() */
        .font_sm      = &lv_font_vec_stub,
        .font_normal  = &lv_font_vec_stub,
        .font_heading = &lv_font_vec_stub,
        .font_large   = &lv_font_vec_stub,
        .font_clock   = &lv_font_vec_stub,
#else
        .font_sm      = &lv_font_montserrat_12,
        .font_normal  = &lv_font_montserrat_14,
        .font_heading = &lv_font_montserrat_16,
        .font_large   = &lv_font_montserrat_22,
        .font_clock   = &lv_font_montserrat_32,
#endif
    },
    /* 1: Dark -- OLED-friendly */
    {
        .primary        = { .blue = 0xFF, .green = 0xA2, .red = 0x42 },  /* 0x42A2FF */
        .primary_dk     = { .blue = 0xE0, .green = 0x80, .red = 0x20 },  /* 0x2080E0 */
        .statusbar      = { .blue = 0x00, .green = 0x00, .red = 0x00 },  /* 0x000000 */
        .statusbar_grad = { .blue = 0x00, .green = 0x00, .red = 0x00 },  /* flat: same as base */
        .navbar         = { .blue = 0x1a, .green = 0x1a, .red = 0x1a },  /* 0x1a1a1a */
        .navbar_grad    = { .blue = 0x1a, .green = 0x1a, .red = 0x1a },  /* flat: same as base */
        .bg             = { .blue = 0x12, .green = 0x12, .red = 0x12 },  /* 0x121212 */
        .bg_grad        = { .blue = 0x12, .green = 0x12, .red = 0x12 },  /* flat: same as base */
        .card           = { .blue = 0x2c, .green = 0x2c, .red = 0x2c },  /* 0x2c2c2c */
        .text           = { .blue = 0xe0, .green = 0xe0, .red = 0xe0 },  /* 0xe0e0e0 */
        .text_sec       = { .blue = 0x9e, .green = 0x9e, .red = 0x9e },  /* 0x9e9e9e */
        .call_green     = { .blue = 0x6d, .green = 0xd0, .red = 0x66 },  /* 0x66D06D */
        .call_red       = { .blue = 0x4d, .green = 0x53, .red = 0xEF },  /* 0xEF534D end-call */
        .missed_red     = { .blue = 0x2f, .green = 0x39, .red = 0xD3 },  /* 0xD3392F missed -- distinct from call_red, parity with Olive */
        .sms_sent       = { .blue = 0x40, .green = 0x5e, .red = 0x2a },  /* 0x2A5E40 */
        .sms_recv       = { .blue = 0x38, .green = 0x38, .red = 0x38 },  /* 0x383838 */
        .indicator      = { .blue = 0x55, .green = 0x55, .red = 0x55 },  /* 0x555555 */
#if LV_USE_FONT_VEC
        .font_sm      = &lv_font_vec_stub,
        .font_normal  = &lv_font_vec_stub,
        .font_heading = &lv_font_vec_stub,
        .font_large   = &lv_font_vec_stub,
        .font_clock   = &lv_font_vec_stub,
#else
        .font_sm      = &lv_font_montserrat_12,
        .font_normal  = &lv_font_montserrat_14,
        .font_heading = &lv_font_montserrat_16,
        .font_large   = &lv_font_montserrat_22,
        .font_clock   = &lv_font_montserrat_32,
#endif
    },
};

#define THEME_COUNT (sizeof(s_themes) / sizeof(s_themes[0]))

static uint32_t s_active_theme_idx = 0;

#if LV_USE_FONT_VEC
static void vec_fonts_init(void)
{
    if(s_vec_fonts_ready) return;
    const lv_font_vec_data_t * text_d = &lv_font_vec_Montserrat_data;
    const lv_font_vec_data_t * icon_d = &lv_font_vec_cellphone_icons_data;
    /* Unified strategy:
     * - All text sizes use the filled Montserrat outlines for cleaner
     *   body text, labels, clocks, and numeric readouts.
     * - The icon subset remains a separate fallback so text does not
     *   carry duplicate FA outlines.
     *
     * Per-font L2 budgets remain configurable so the MCU profile can spend
     * less SRAM than the SDL config without changing any rendered pixels.
     *
     * The other three fonts are disabled outright -- their callers are
     * predominantly static after first paint, so the cached bitmap +
     * LRU node + RB-tree overhead per glyph costs more SRAM than
     * on-demand rasterization costs CPU:
     *
     *   - heading (16 px): contacts header `+`, list section headers,
     *     dialer Call/Backspace buttons, photo header, settings page
     *     labels. Rasterizes per screen push; framebuffer caches the
     *     result for subsequent frames.
     *   - large  (22 px): calc display, dialer dialed-number, dialer
     *     keypad button labels, contacts in-call number, settings time
     *     rollers. These are interactive surfaces with repeated digit
     *     updates, so leaving this size uncached makes taps visibly
     *     more expensive than the static-screen profile suggested.
     *     Give it a modest L2 budget to smooth number-entry paths
     *     without pushing SRAM cost into the clock-size range.
     *   - clock  (32 px): lock-screen + statusbar clocks (10 s / 60 s
     *     timers), music now-playing time (track-change rate). Lowest
     *     repaint frequency in the demo.
     *
     * Repeated characters in a single label ("00:00") with the cache
     * disabled rasterize once per occurrence rather than per unique
     * glyph; profile if a workload changes. */
    /* Init-result checking: lv_font_vec_init_ex returns LV_RESULT_INVALID
     * on allocator failure or invalid pixel size, leaving the lv_font_t
     * zeroed. Without checking, a later draw would deref a NULL
     * get_glyph_dsc via lv_font_get_glyph_dsc_fmt -> fallback chain. */
    /* Text faces. Aligned with `text_caches` and `text_sizes` below. */
    lv_font_t * const text_fonts[] = {
        &s_vec_font_12, &s_vec_font_14, &s_vec_font_16,
        &s_vec_font_22, &s_vec_font_32,
    };
    static const int32_t text_sizes[] = { 12, 14, 16, 22, 32 };
    static const uint32_t text_caches[] = {
        LV_DEMO_CELLPHONE_FONT_CACHE_SM,
        LV_DEMO_CELLPHONE_FONT_CACHE_NORMAL,
        LV_DEMO_CELLPHONE_FONT_CACHE_HEADING,
        LV_DEMO_CELLPHONE_FONT_CACHE_LARGE,
        LV_DEMO_CELLPHONE_FONT_CACHE_CLOCK,
    };
    /* Icon faces: only sizes the demo actually paints LV_SYMBOL_* at.
     * Index 0 here pairs with text_fonts[1] (size 14, &s_vec_font_14),
     * not text_fonts[0] -- s_vec_font_12 has no fallback by design. */
    lv_font_t * const icon_fonts[] = {
        &s_vec_icon_font_14, &s_vec_icon_font_16,
        &s_vec_icon_font_22, &s_vec_icon_font_32,
    };
    static const int32_t icon_sizes[] = { 14, 16, 22, 32 };
    static const uint32_t icon_text_idx[] = { 1, 2, 3, 4 };
    const uint32_t n_text = sizeof(text_sizes) / sizeof(text_sizes[0]);
    const uint32_t n_icon = sizeof(icon_sizes) / sizeof(icon_sizes[0]);
    uint32_t text_ok = 0, icon_ok = 0;

    for(uint32_t i = 0; i < n_text; i++) {
        if(lv_font_vec_init_ex(text_fonts[i], text_d, text_sizes[i], text_caches[i])
           != LV_RESULT_OK) goto fail;
        text_ok = i + 1;
    }
    /* Icon fallback face: sparse FA5 subset, L2 disabled (cache_size=0)
     * because these glyphs are mostly static after first paint. */
    for(uint32_t i = 0; i < n_icon; i++) {
        if(lv_font_vec_init_ex(icon_fonts[i], icon_d, icon_sizes[i], 0) != LV_RESULT_OK) goto fail;
        icon_ok = i + 1;
        text_fonts[icon_text_idx[i]]->fallback = icon_fonts[i];
    }

    /* Patch every theme's font slots so cellphone_theme_active() can
     * return a direct pointer instead of rebuilding a copy each call. */
    for(uint32_t i = 0; i < THEME_COUNT; i++) {
        s_themes[i].font_sm      = &s_vec_font_12;
        s_themes[i].font_normal  = &s_vec_font_14;
        s_themes[i].font_heading = &s_vec_font_16;
        s_themes[i].font_large   = &s_vec_font_22;
        s_themes[i].font_clock   = &s_vec_font_32;
    }

    s_vec_fonts_ready = true;
    return;

fail:
    /* Unwind: deinit any fonts we already brought up so a retry from a
     * fresh allocator state starts clean. lv_font_vec_deinit is a no-op
     * on a font whose dsc is still NULL (the failed call's target). */
    LV_LOG_ERROR("vec font init failed (text_ok=%u icon_ok=%u)",
                 (unsigned)text_ok, (unsigned)icon_ok);
    for(uint32_t i = 0; i < icon_ok; i++) lv_font_vec_deinit(icon_fonts[i]);
    for(uint32_t i = 0; i < text_ok; i++) {
        text_fonts[i]->fallback = NULL;
        lv_font_vec_deinit(text_fonts[i]);
    }
}
#endif /* LV_USE_FONT_VEC */

const cellphone_motion_preset_t CELLPHONE_MOTION_STANDARD   = { 240, 220, lv_anim_path_ease_in_out };
const cellphone_motion_preset_t CELLPHONE_MOTION_EMPHASIZED = { 320, 260, lv_anim_path_ease_in_out };
const cellphone_motion_preset_t CELLPHONE_MOTION_QUICK      = { 120, 110, lv_anim_path_ease_out };

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void create_demo(lv_obj_t * parent,
                        const cellphone_screen_create_fn * initial_stack,
                        const screen_refresh_state_t * initial_states,
                        uint32_t initial_depth);
static void create_stack_sequence(const cellphone_screen_create_fn * fns,
                                  const screen_refresh_state_t * states,
                                  uint32_t depth);
static void create_hosts(lv_obj_t * host_parent);
static void apply_pressed_feedback_recursive(lv_obj_t * obj);
static void screen_delete_timer_cb(lv_timer_t * timer);
#if defined(LV_DEMO_CELLPHONE_SKIN) && LV_DEMO_CELLPHONE_SKIN && LV_USE_SDL
    static bool parent_is_active_screen(lv_obj_t * parent);
    static void prepare_skin_parent(lv_obj_t * parent);
#endif
static void idle_timer_cb(lv_timer_t * timer);
static screen_refresh_state_t screen_refresh_state_capture(cellphone_screen_create_fn fn,
                                                           lv_obj_t * screen);
static void screen_refresh_state_restore(cellphone_screen_create_fn fn,
                                         lv_obj_t * screen,
                                         const screen_refresh_state_t * state);
static bool screen_create_fn_is_game(cellphone_screen_create_fn fn);
static lv_obj_t * screen_find_scrollable(lv_obj_t * root);

/**********************
 *  STATIC VARIABLES
 **********************/

/** Screen stack: holds the create function and live screen pointer. */
static struct {
    cellphone_screen_create_fn create_fn;
    lv_obj_t * screen;
} s_stack[CELLPHONE_SCREEN_STACK_DEPTH];

static int s_stack_top = -1;
static lv_obj_t * s_root;
static lv_obj_t * s_stack_host;
static lv_obj_t * s_overlay_host;
static lv_timer_t * s_idle_timer;
static lv_demo_args_t s_demo_args;
static bool s_demo_args_valid;
static bool s_screen_transitions_enabled = true;
static uint32_t s_screen_event_shown;
static uint32_t s_screen_event_hidden;

/** Global pressed-feedback style: darken interactive widgets on touch. */
static lv_style_t s_pressed_style;
static bool s_pressed_style_inited;

/* Modern phone-UI palette: flat saturated tones tuned for white glyphs
 * (Tailwind 500-tier) so the icon grid reads cleanly on both Olive and
 * Dark themes without per-theme palette flips. Apps either map to a
 * bundled FA5 glyph (icon_text) or to a procedural drawer (glyph_draw)
 * -- never to a text monogram. */
#define CELLPHONE_APP_LIST(X) \
    X("Phone",      LV_SYMBOL_CALL,     NULL,                       0x22C55E, cellphone_dialer_create) \
    X("Contacts",   LV_SYMBOL_LIST,     NULL,                       0x3B82F6, cellphone_contacts_create) \
    X("Messages",   NULL,               cellphone_glyph_messages,   0x10B981, cellphone_sms_create) \
    X("Calculator", NULL,               cellphone_glyph_calculator, 0xF59E0B, cellphone_calc_create) \
    X("Music",      LV_SYMBOL_AUDIO,    NULL,                       0xEC4899, cellphone_music_create) \
    X("Photos",     LV_SYMBOL_IMAGE,    NULL,                       0xF97316, cellphone_photo_create) \
    X("Camera",     NULL,               cellphone_glyph_camera,     0x6B7280, cellphone_camera_create) \
    X("Settings",   LV_SYMBOL_SETTINGS, NULL,                       0x64748B, cellphone_settings_create) \
    X("Call Log",   LV_SYMBOL_REFRESH,  NULL,                       0x8B5CF6, cellphone_calllog_create) \
    X("Snake",      NULL,               cellphone_glyph_snake,      0x14B8A6, cellphone_snake_create) \
    X("Pong",       NULL,               cellphone_glyph_pong,       0x6366F1, cellphone_pong_create) \
    X("Tetris",     NULL,               cellphone_glyph_tetris,     0xEF4444, cellphone_tetris_create)

/** App registry -- order matches the home screen grid. */
static const cellphone_app_entry_t s_apps[] = {
#define X(name, icon_text, glyph_draw, color_hex, create_fn) \
    { name, icon_text, glyph_draw, color_hex, create_fn },
    CELLPHONE_APP_LIST(X)
#undef X
};

#define APP_COUNT (sizeof(s_apps) / sizeof(s_apps[0]))

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

/*---------------------------------------------------------------------------
 * Real-time clock helpers
 *---------------------------------------------------------------------------*/

static const char * const s_wday_names[] = {
    "Sunday", "Monday", "Tuesday", "Wednesday",
    "Thursday", "Friday", "Saturday"
};

static const char * const s_mon_names[] = {
    "January", "February", "March", "April", "May", "June",
    "July", "August", "September", "October", "November", "December"
};

void cellphone_time_now(char * buf, uint32_t buf_len)
{
    time_t t = time(NULL);
    struct tm * tm = localtime(&t);
    if(tm) {
        lv_snprintf(buf, buf_len, "%02d:%02d", tm->tm_hour, tm->tm_min);
    }
    else {
        lv_snprintf(buf, buf_len, "--:--");
    }
}

void cellphone_date_now(char * buf, uint32_t buf_len)
{
    time_t t = time(NULL);
    struct tm * tm = localtime(&t);
    if(tm) {
        lv_snprintf(buf, buf_len, "%s, %s %d",
                    s_wday_names[tm->tm_wday % 7],
                    s_mon_names[tm->tm_mon % 12],
                    tm->tm_mday);
    }
    else {
        lv_snprintf(buf, buf_len, "---");
    }
}

/*---------------------------------------------------------------------------
 * Theme API
 *---------------------------------------------------------------------------*/

uint32_t cellphone_theme_count(void)
{
    return THEME_COUNT;
}

const cellphone_theme_t * cellphone_theme_get(uint32_t idx)
{
    if(idx >= THEME_COUNT) return &s_themes[0];
    return &s_themes[idx];
}

const cellphone_theme_t * cellphone_theme_active(void)
{
    /* Font pointers were patched in by vec_fonts_init() (when LV_USE_FONT_VEC),
     * so a direct pointer suffices and we avoid copying a struct per call. */
    return &s_themes[s_active_theme_idx];
}

uint32_t cellphone_theme_active_index(void)
{
    return s_active_theme_idx;
}

void cellphone_theme_set(uint32_t idx)
{
    if(idx >= THEME_COUNT) return;
    s_active_theme_idx = idx;
}

uint32_t cellphone_screen_event_shown(void)
{
    if(s_screen_event_shown == 0) s_screen_event_shown = lv_event_register_id();
    return s_screen_event_shown;
}

uint32_t cellphone_screen_event_hidden(void)
{
    if(s_screen_event_hidden == 0) s_screen_event_hidden = lv_event_register_id();
    return s_screen_event_hidden;
}

/*---------------------------------------------------------------------------
 * Public API
 *---------------------------------------------------------------------------*/

/**
 * One-time-per-entry init: vector font cache + LVGL default theme.
 *
 * Must be called before every public entry point AND every time the active
 * theme changes, otherwise LVGL-internal widgets (lv_menu headers, dropdown
 * popups, slider/roller accents) keep the old theme's primary color and
 * the demo config's stub default font.
 */
static void demo_default_theme_sync(void)
{
#if LV_USE_FONT_VEC
    vec_fonts_init();
    if(!s_vec_fonts_ready) {
        /* Init failed (allocator). Don't hand a NULL-callback font to the
         * theme: that would crash on the first paint via the fallback
         * walk in lv_font_get_glyph_dsc_fmt. Leaving the theme on its
         * existing font keeps the system alive (text renders via the
         * zero-glyph stub) so a higher level can recover. */
        return;
    }

    /* Demo config sets LV_FONT_DEFAULT to a zero-glyph stub; route LVGL's
     * built-in widgets through the real vec font and the active palette. */
    const cellphone_theme_t * t = cellphone_theme_active();
    lv_theme_default_init(NULL, t->primary, t->primary_dk,
                          s_active_theme_idx == 1 /* dark mode */,
                          &s_vec_font_14);
#endif
}

void lv_demo_cellphone(void)
{
    demo_default_theme_sync();

    lv_demo_args_t args;
    lv_demo_args_init(&args);
    lv_demo_cellphone_with_args(&args);
}

void lv_demo_cellphone_with_args(const lv_demo_args_t * args)
{
    static const cellphone_screen_create_fn boot_stack[] = {
        cellphone_home_create,
        cellphone_lock_create
    };

    LV_ASSERT_NULL(args);

    /* Refresh in case `with_args` was invoked directly (embedded entry) or in
     * case the active theme changed since the last call (rebuild path). */
    demo_default_theme_sync();
    cellphone_data_reset_mutable();

    s_demo_args = *args;
    s_demo_args_valid = true;

    if(s_root && lv_obj_is_valid(s_root)) {
#if defined(LV_DEMO_CELLPHONE_SKIN) && LV_DEMO_CELLPHONE_SKIN && LV_USE_SDL
        cellphone_skin_reset();
#endif
        lv_obj_delete(s_root);
    }

    if(s_idle_timer) {
        lv_timer_delete(s_idle_timer);
        s_idle_timer = NULL;
    }

    s_stack_top = -1;
    s_root = NULL;
    s_stack_host = NULL;
    s_overlay_host = NULL;
    create_demo(args->parent ? args->parent : lv_screen_active(),
                boot_stack, NULL, 2);
}

void lv_demo_cellphone_rebuild(void)
{
    lv_demo_args_t args;

    if(s_demo_args_valid) {
        args = s_demo_args;
    }
    else {
        lv_demo_args_init(&args);
    }

    lv_demo_cellphone_with_args(&args);
}

void lv_demo_cellphone_refresh_theme(void)
{
    lv_demo_args_t args;
    cellphone_screen_create_fn stack_fns[CELLPHONE_SCREEN_STACK_DEPTH];
    screen_refresh_state_t stack_states[CELLPHONE_SCREEN_STACK_DEPTH];
    uint32_t depth = 0;
    static const cellphone_screen_create_fn boot_stack[] = {
        cellphone_home_create,
        cellphone_lock_create
    };

    if(s_demo_args_valid) {
        args = s_demo_args;
    }
    else {
        lv_demo_args_init(&args);
    }

    if(s_stack_top >= 0) {
        depth = (uint32_t)(s_stack_top + 1);
        if(depth > CELLPHONE_SCREEN_STACK_DEPTH) depth = CELLPHONE_SCREEN_STACK_DEPTH;
        for(uint32_t i = 0; i < depth; i++) {
            stack_fns[i] = s_stack[i].create_fn;
            stack_states[i] = screen_refresh_state_capture(s_stack[i].create_fn,
                                                           s_stack[i].screen);
        }
    }

    demo_default_theme_sync();

    if(s_root && lv_obj_is_valid(s_root)) {
#if defined(LV_DEMO_CELLPHONE_SKIN) && LV_DEMO_CELLPHONE_SKIN && LV_USE_SDL
        cellphone_skin_reset();
#endif
        lv_obj_delete(s_root);
    }

    if(s_idle_timer) {
        lv_timer_delete(s_idle_timer);
        s_idle_timer = NULL;
    }

    s_stack_top = -1;
    s_root = NULL;
    s_stack_host = NULL;
    s_overlay_host = NULL;
    s_demo_args = args;
    s_demo_args_valid = true;
    create_demo(args.parent ? args.parent : lv_screen_active(),
                depth ? stack_fns : boot_stack,
                depth ? stack_states : NULL,
                depth ? depth : 2);
}

const cellphone_app_entry_t * cellphone_app_registry(uint32_t * count)
{
    if(count) *count = APP_COUNT;
    return s_apps;
}

/*---------------------------------------------------------------------------
 * UI helpers
 *---------------------------------------------------------------------------*/

lv_obj_t * cellphone_obj_bare(lv_obj_t * parent)
{
    lv_obj_t * obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    return obj;
}

void cellphone_obj_paint_fill(lv_obj_t * obj, lv_color_t color)
{
    lv_obj_set_style_bg_color(obj, color, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
}

lv_obj_t * cellphone_obj_fill(lv_obj_t * parent, lv_color_t color)
{
    lv_obj_t * obj = cellphone_obj_bare(parent);
    cellphone_obj_paint_fill(obj, color);
    return obj;
}

lv_obj_t * cellphone_obj_transparent(lv_obj_t * parent)
{
    lv_obj_t * obj = cellphone_obj_bare(parent);
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    return obj;
}

void cellphone_obj_paint_grad(lv_obj_t * obj, lv_color_t top, lv_color_t bot)
{
    lv_obj_set_style_bg_color(obj, top, 0);
    lv_obj_set_style_bg_grad_color(obj, bot, 0);
    lv_obj_set_style_bg_grad_dir(obj, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
}

lv_obj_t * cellphone_obj_bar(lv_obj_t * parent, lv_color_t top, lv_color_t bot)
{
    lv_obj_t * obj = cellphone_obj_bare(parent);
    cellphone_obj_paint_grad(obj, top, bot);
    return obj;
}

lv_obj_t * cellphone_section_header(lv_obj_t * parent, const char * title,
                                    lv_align_t title_align,
                                    const char * trailing_text)
{
    lv_obj_t * header = cellphone_obj_bar(parent, CELLPHONE_COLOR_NAVBAR_GRAD,
                                          CELLPHONE_COLOR_NAVBAR);
    lv_obj_set_size(header, CELLPHONE_CONTENT_W, CELLPHONE_SECTION_HDR_H);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    /* Inset the title only when it sits against an edge so centered titles
     * don't pick up a stray 8 px offset. */
    int32_t x_ofs = (title_align == LV_ALIGN_LEFT_MID)  ?  8
                    : (title_align == LV_ALIGN_RIGHT_MID) ? -8
                    : 0;
    lv_obj_t * title_lbl = cellphone_label(header, title,
                                           CELLPHONE_FONT_SM, CELLPHONE_COLOR_TEXT);
    lv_obj_align(title_lbl, title_align, x_ofs, 0);

    if(trailing_text && trailing_text[0] != '\0') {
        lv_obj_t * trailing = cellphone_label(header, trailing_text,
                                              CELLPHONE_FONT_HEADING, CELLPHONE_COLOR_TEXT);
        lv_obj_align(trailing, LV_ALIGN_RIGHT_MID, -8, 0);
    }

    return header;
}

lv_obj_t * cellphone_label(lv_obj_t * parent, const char * text,
                           const lv_font_t * font, lv_color_t color)
{
    lv_obj_t * lbl = lv_label_create(parent);
    if(text) lv_label_set_text(lbl, text);
    lv_obj_set_style_text_font(lbl, font, 0);
    lv_obj_set_style_text_color(lbl, color, 0);
    return lbl;
}

/*---------------------------------------------------------------------------
 * Screen stack
 *---------------------------------------------------------------------------*/

void cellphone_screen_push(cellphone_screen_create_fn fn)
{
    if(s_stack_top >= CELLPHONE_SCREEN_STACK_DEPTH - 1) return;
    if(!fn) return;
    if(!s_stack_host) return;

    lv_obj_update_layout(s_stack_host);

    /* Create a new page within the demo root so the demo can run embedded.
     * Strip all default theme styles (padding, border, radius) so the page
     * fills s_stack_host edge-to-edge without clipping its children. */
    lv_obj_t * scr = cellphone_obj_transparent(s_stack_host);
    lv_obj_set_pos(scr, 0, 0);
    lv_obj_set_size(scr, lv_pct(100), lv_pct(100));
    /* Subtle vertical gradient so the LCD area reads as a glossy panel, not a
     * flat block of color. Both stops come from the active theme; for themes
     * where a flat look is desired, set bg_grad equal to bg. */
    cellphone_obj_paint_grad(scr, CELLPHONE_COLOR_BG, CELLPHONE_COLOR_BG_GRAD);

    /*
     * Content area: sits between status bar and nav bar.
     * We create a container at the right position so the app's
     * create function doesn't need to know about chrome geometry.
     */
    lv_obj_t * content = cellphone_obj_transparent(scr);
    lv_obj_set_pos(content, 0, CELLPHONE_CONTENT_Y);
    lv_obj_set_size(content, CELLPHONE_CONTENT_W, CELLPHONE_CONTENT_H);

    /* Let the app populate the content area */
    fn(content);
    apply_pressed_feedback_recursive(scr);

    lv_obj_t * prev_scr = s_stack_top >= 0 ? s_stack[s_stack_top].screen : NULL;

    /* Push onto stack */
    s_stack_top++;
    s_stack[s_stack_top].create_fn = fn;
    s_stack[s_stack_top].screen    = scr;

    lv_obj_send_event(scr, (lv_event_code_t)cellphone_screen_event_shown(), NULL);
    if(prev_scr) {
        lv_obj_send_event(prev_scr, (lv_event_code_t)cellphone_screen_event_hidden(), NULL);
    }

    if(s_overlay_host && !lv_obj_has_flag(s_overlay_host, LV_OBJ_FLAG_HIDDEN)) {
        lv_obj_move_foreground(s_overlay_host);
    }

    cellphone_navbar_refresh();

    if(prev_scr && s_screen_transitions_enabled) {
        int32_t width = lv_obj_get_width(s_stack_host);
        if(width <= 0) width = CELLPHONE_HOR_RES;
        lv_obj_set_x(scr, width);
        cellphone_anim_run(prev_scr, cellphone_anim_set_x_cb,
                           0, -width, CELLPHONE_MOTION_STANDARD.enter_ms,
                           CELLPHONE_MOTION_STANDARD.path_cb);
        cellphone_anim_run(scr, cellphone_anim_set_x_cb,
                           width, 0, CELLPHONE_MOTION_STANDARD.enter_ms,
                           CELLPHONE_MOTION_STANDARD.path_cb);
    }
    else {
        lv_obj_set_x(scr, 0);
    }
}

void cellphone_screen_pop(void)
{
    if(s_stack_top <= 0) return;  /* don't pop the home screen */

    lv_obj_update_layout(s_stack_host);

    lv_obj_t * old_scr = s_stack[s_stack_top].screen;
    lv_obj_t * prev_scr = s_stack[s_stack_top - 1].screen;
    int32_t width = lv_obj_get_width(s_stack_host);
    if(width <= 0) width = CELLPHONE_HOR_RES;

    /* Drop the stack reference immediately so reopen paths cannot touch the
     * old screen through singleton state while it slides out. */
    s_stack[s_stack_top].screen = NULL;
    s_stack_top--;

    lv_obj_send_event(old_scr, (lv_event_code_t)cellphone_screen_event_hidden(), NULL);
    lv_obj_send_event(prev_scr, (lv_event_code_t)cellphone_screen_event_shown(), NULL);

    if(s_overlay_host && !lv_obj_has_flag(s_overlay_host, LV_OBJ_FLAG_HIDDEN)) {
        lv_obj_move_foreground(s_overlay_host);
    }

    cellphone_navbar_refresh();

    if(s_screen_transitions_enabled) {
        lv_obj_set_x(prev_scr, -width);
        cellphone_anim_run(prev_scr, cellphone_anim_set_x_cb,
                           -width, 0, CELLPHONE_MOTION_STANDARD.exit_ms,
                           CELLPHONE_MOTION_STANDARD.path_cb);
        cellphone_anim_run(old_scr, cellphone_anim_set_x_cb,
                           0, width, CELLPHONE_MOTION_STANDARD.exit_ms,
                           CELLPHONE_MOTION_STANDARD.path_cb);

        /* Let the slide-out finish, then destroy the old screen synchronously so
         * its delete callbacks run before the user can reopen the same app. */
        lv_timer_t * delete_timer = lv_timer_create(screen_delete_timer_cb,
                                                    CELLPHONE_MOTION_STANDARD.exit_ms + 50,
                                                    old_scr);
        if(delete_timer) lv_timer_set_repeat_count(delete_timer, 1);
        else lv_obj_delete(old_scr);
    }
    else {
        lv_obj_set_x(prev_scr, 0);
        lv_obj_delete(old_scr);
    }
}

void cellphone_screen_home(void)
{
    if(s_stack_top <= 0) return;

    /* Cancel any running X animation on the home screen before resetting */
    lv_anim_delete(s_stack[0].screen, cellphone_anim_set_x_cb);
    lv_obj_set_x(s_stack[0].screen, 0);

    /* Delete everything above the home screen */
    for(int i = s_stack_top; i > 0; i--) {
        lv_obj_t * scr = s_stack[i].screen;
        lv_obj_send_event(scr, (lv_event_code_t)cellphone_screen_event_hidden(), NULL);
        if(s_screen_transitions_enabled) {
            lv_obj_fade_out(scr, CELLPHONE_MOTION_QUICK.exit_ms, 0);
            lv_timer_t * delete_timer = lv_timer_create(screen_delete_timer_cb,
                                                        CELLPHONE_MOTION_QUICK.exit_ms + 50,
                                                        scr);
            if(delete_timer) lv_timer_set_repeat_count(delete_timer, 1);
            else lv_obj_delete(scr);
        }
        else {
            lv_obj_delete(scr);
        }
        s_stack[i].screen = NULL;
    }
    s_stack_top = 0;
    lv_obj_send_event(s_stack[0].screen, (lv_event_code_t)cellphone_screen_event_shown(), NULL);

    cellphone_navbar_refresh();
}

void cellphone_screen_set_transitions_enabled(bool enabled)
{
    s_screen_transitions_enabled = enabled;
}

int cellphone_screen_depth(void)
{
    return s_stack_top + 1;
}

lv_obj_t * cellphone_screen_top(void)
{
    if(s_stack_top < 0) return NULL;
    return s_stack[s_stack_top].screen;
}

bool cellphone_screen_top_is(cellphone_screen_create_fn fn)
{
    if(s_stack_top < 0 || !fn) return false;
    return s_stack[s_stack_top].create_fn == fn;
}

void cellphone_chrome_set_visible(bool visible)
{
    if(!s_overlay_host) return;
    if(visible) {
        lv_obj_remove_flag(s_overlay_host, LV_OBJ_FLAG_HIDDEN);
    }
    else {
        lv_obj_add_flag(s_overlay_host, LV_OBJ_FLAG_HIDDEN);
    }
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/**
 * Build the initial demo: home screen + chrome overlays.
 * When LV_DEMO_CELLPHONE_SKIN is active, the skin layer draws
 * a phone bezel and provides an LCD container for the demo content.
 */
static void create_demo(lv_obj_t * parent,
                        const cellphone_screen_create_fn * initial_stack,
                        const screen_refresh_state_t * initial_states,
                        uint32_t initial_depth)
{
    /* Touch feedback: darken clickable widgets on press (material 16% overlay).
     * Apply this only to widgets created inside the cellphone demo. */
    if(!s_pressed_style_inited) {
        lv_style_init(&s_pressed_style);
        s_pressed_style_inited = true;
    }
    lv_style_set_bg_color(&s_pressed_style, CELLPHONE_COLOR_TEXT);
    lv_style_set_bg_opa(&s_pressed_style, LV_OPA_20);

#if defined(LV_DEMO_CELLPHONE_SKIN) && LV_DEMO_CELLPHONE_SKIN && LV_USE_SDL
    const cellphone_skin_t * skin = cellphone_skin_active();
    bool use_skin = parent_is_active_screen(parent);

    if(use_skin) {
        prepare_skin_parent(parent);
    }
#endif

    s_root = cellphone_obj_transparent(parent);
#if defined(LV_DEMO_CELLPHONE_SKIN) && LV_DEMO_CELLPHONE_SKIN && LV_USE_SDL
    if(use_skin) {
        lv_obj_set_size(s_root, skin->frame_w, skin->frame_h);
    }
    else {
        lv_obj_set_size(s_root, lv_pct(100), lv_pct(100));
    }
#else
    lv_obj_set_size(s_root, lv_pct(100), lv_pct(100));
#endif
    cellphone_obj_paint_fill(s_root, CELLPHONE_COLOR_BG);

    /*
     * Determine where demo content lives. In skin mode, the skin layer
     * draws the phone body and returns an LCD-sized container; otherwise
     * content goes directly into the root.
     */
    lv_obj_t * host_parent = s_root;
#if defined(LV_DEMO_CELLPHONE_SKIN) && LV_DEMO_CELLPHONE_SKIN && LV_USE_SDL
    if(use_skin) {
        host_parent = cellphone_skin_create(s_root);
    }
#endif

    create_hosts(host_parent);

    /* Create persistent overlays inside the demo root first */
    cellphone_statusbar_create(s_overlay_host);
    cellphone_navbar_create(s_overlay_host);
    apply_pressed_feedback_recursive(s_overlay_host);
    lv_obj_move_foreground(s_overlay_host);

    create_stack_sequence(initial_stack, initial_states, initial_depth);

    /* Idle timer: auto-lock after 30s of inactivity */
    if(s_idle_timer) lv_timer_delete(s_idle_timer);
    s_idle_timer = lv_timer_create(idle_timer_cb, IDLE_POLL_PERIOD_MS, NULL);
}

/**
 * Create the stack host and overlay host under the given parent.
 * Factored out of create_demo to avoid repeating the same setup
 * in three conditional branches.
 */
static void create_hosts(lv_obj_t * host_parent)
{
    s_stack_host = cellphone_obj_transparent(host_parent);
    lv_obj_set_pos(s_stack_host, 0, 0);
    lv_obj_set_size(s_stack_host, lv_pct(100), lv_pct(100));

    s_overlay_host = cellphone_obj_transparent(host_parent);
    lv_obj_set_pos(s_overlay_host, 0, 0);
    lv_obj_set_size(s_overlay_host, lv_pct(100), lv_pct(100));
    lv_obj_clear_flag(s_overlay_host, LV_OBJ_FLAG_CLICKABLE);
}

static void create_stack_sequence(const cellphone_screen_create_fn * fns,
                                  const screen_refresh_state_t * states,
                                  uint32_t depth)
{
    if(!fns || depth == 0) return;

    bool transitions_enabled = s_screen_transitions_enabled;
    cellphone_screen_set_transitions_enabled(false);
    for(uint32_t i = 0; i < depth; i++) {
        if(fns[i]) {
            cellphone_screen_push(fns[i]);
            screen_refresh_state_restore(fns[i], s_stack[s_stack_top].screen,
                                         states ? &states[i] : NULL);
        }
    }
    cellphone_screen_set_transitions_enabled(transitions_enabled);
}

static screen_refresh_state_t screen_refresh_state_capture(cellphone_screen_create_fn fn,
                                                           lv_obj_t * screen)
{
    screen_refresh_state_t state;
    lv_obj_t * scroll;

    lv_memzero(&state, sizeof(state));

    if(fn == cellphone_settings_create) {
        state.screen_state = cellphone_settings_refresh_state_capture();
    }
    else if(fn == cellphone_home_create) {
        state.screen_state = cellphone_home_refresh_state_capture();
    }
    else if(screen_create_fn_is_game(fn)) {
        state.has_game_state = cellphone_game_refresh_state_capture(screen,
                                                                    state.game_state,
                                                                    sizeof(state.game_state));
    }

    scroll = screen_find_scrollable(screen);
    if(scroll) {
        state.has_scroll = true;
        state.scroll_x = lv_obj_get_scroll_x(scroll);
        state.scroll_y = lv_obj_get_scroll_y(scroll);
    }

    return state;
}

static void screen_refresh_state_restore(cellphone_screen_create_fn fn,
                                         lv_obj_t * screen,
                                         const screen_refresh_state_t * state)
{
    lv_obj_t * scroll;

    if(state == NULL) return;

    if(fn == cellphone_settings_create) {
        cellphone_settings_refresh_state_restore(state->screen_state);
    }
    else if(fn == cellphone_home_create) {
        cellphone_home_refresh_state_restore(state->screen_state);
    }
    else if(state->has_game_state && screen_create_fn_is_game(fn)) {
        cellphone_game_refresh_state_restore(screen, state->game_state,
                                             sizeof(state->game_state));
    }

    if(state->has_scroll) {
        scroll = screen_find_scrollable(screen);
        if(scroll) lv_obj_scroll_to(scroll, state->scroll_x, state->scroll_y, LV_ANIM_OFF);
    }
}

static bool screen_create_fn_is_game(cellphone_screen_create_fn fn)
{
    return fn == cellphone_snake_create
           || fn == cellphone_pong_create
           || fn == cellphone_tetris_create;
}

static lv_obj_t * screen_find_scrollable(lv_obj_t * root)
{
    if(root == NULL) return NULL;

    if(lv_obj_has_flag(root, LV_OBJ_FLAG_SCROLLABLE) &&
       lv_obj_get_scroll_dir(root) != LV_DIR_NONE) {
        return root;
    }

    uint32_t child_count = lv_obj_get_child_count(root);
    for(uint32_t i = 0; i < child_count; i++) {
        lv_obj_t * found = screen_find_scrollable(lv_obj_get_child(root, i));
        if(found) return found;
    }

    return NULL;
}

static void screen_delete_timer_cb(lv_timer_t * timer)
{
    lv_obj_t * obj = lv_timer_get_user_data(timer);
    lv_timer_delete(timer);
    if(obj && lv_obj_is_valid(obj)) {
        lv_obj_delete(obj);
    }
}

#if defined(LV_DEMO_CELLPHONE_SKIN) && LV_DEMO_CELLPHONE_SKIN && LV_USE_SDL
static bool parent_is_active_screen(lv_obj_t * parent)
{
    lv_display_t * disp = lv_obj_get_display(parent);

    if(disp == NULL) return false;

    return parent == lv_display_get_screen_active(disp);
}

static void prepare_skin_parent(lv_obj_t * parent)
{
    const cellphone_skin_t * skin = cellphone_skin_active();
    lv_display_t * disp = lv_obj_get_display(parent);

    if(disp == NULL || skin == NULL) return;

    if(parent_is_active_screen(parent)) {
        lv_display_set_resolution(disp, skin->frame_w, skin->frame_h);
    }
}
#endif

/**
 * Idle timer: polls display inactivity every 1s.
 * When idle > 30s and the lock screen is not already showing, push it.
 */
static void idle_timer_cb(lv_timer_t * timer)
{
    LV_UNUSED(timer);

    /* Don't lock if lock screen is already on top */
    if(cellphone_screen_top_is(cellphone_lock_create)) return;

    uint32_t idle = lv_display_get_inactive_time(NULL);
    if(idle >= IDLE_LOCK_TIMEOUT_MS) {
        cellphone_screen_push(cellphone_lock_create);
    }
}

/**
 * Theme apply callback: add pressed-state feedback style to
 * every clickable widget (buttons, list items, menu items, etc.).
 * Material Design uses 16% opacity overlay on press.
 */
static void apply_pressed_feedback_recursive(lv_obj_t * obj)
{
    if(obj == NULL) return;

    /* Button matrixes manage press feedback per-item. Applying the demo's
     * whole-object pressed overlay to the matrix itself makes the entire
     * keypad flash on every tap. */
    if(lv_obj_has_flag(obj, LV_OBJ_FLAG_CLICKABLE) &&
       !lv_obj_has_class(obj, &lv_buttonmatrix_class)) {
        lv_obj_add_style(obj, &s_pressed_style, LV_STATE_PRESSED);
    }

    uint32_t child_count = lv_obj_get_child_count(obj);
    for(uint32_t i = 0; i < child_count; i++) {
        apply_pressed_feedback_recursive(lv_obj_get_child(obj, i));
    }
}

#endif /* LV_USE_DEMO_CELLPHONE */
