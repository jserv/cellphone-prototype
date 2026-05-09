/**
 * @file lv_demo_cellphone_common.h
 *
 * Shared constants, geometry, colors, and types for the Cell Phone Demo.
 * Default geometry is portrait QVGA (240x320), with an optional large mode.
 */

#ifndef LV_DEMO_CELLPHONE_COMMON_H
#define LV_DEMO_CELLPHONE_COMMON_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#include "../../lvgl.h"

#ifndef LV_DEMO_CELLPHONE_LARGE
#define LV_DEMO_CELLPHONE_LARGE 0
#endif

/*********************
 * SCREEN GEOMETRY
 *********************/
#if LV_DEMO_CELLPHONE_LARGE
#define CELLPHONE_HOR_RES       480
#define CELLPHONE_VER_RES       320
#define CELLPHONE_STATUSBAR_H   28
#define CELLPHONE_NAVBAR_H      52
#define CELLPHONE_ICON_SIZE     56
#define CELLPHONE_ICON_LABEL_H  14
#define CELLPHONE_GRID_COLS     4
#define CELLPHONE_GRID_ROWS     3
#define CELLPHONE_LIST_ROW_H    48
#define CELLPHONE_SECTION_HDR_H 28
#define CELLPHONE_BTN_MIN       56
#else
/* Portrait QVGA: 240 x 320 */
#define CELLPHONE_HOR_RES       240
#define CELLPHONE_VER_RES       320
#define CELLPHONE_STATUSBAR_H   20
#define CELLPHONE_NAVBAR_H      42
#define CELLPHONE_ICON_SIZE     40
#define CELLPHONE_ICON_LABEL_H  12
#define CELLPHONE_GRID_COLS     3
#define CELLPHONE_GRID_ROWS     3
#define CELLPHONE_LIST_ROW_H    40
#define CELLPHONE_SECTION_HDR_H 22
#define CELLPHONE_BTN_MIN       44
#endif

#define CELLPHONE_CONTENT_Y     CELLPHONE_STATUSBAR_H
#define CELLPHONE_CONTENT_H     (CELLPHONE_VER_RES - CELLPHONE_STATUSBAR_H - CELLPHONE_NAVBAR_H)
#define CELLPHONE_CONTENT_W     CELLPHONE_HOR_RES
#define CELLPHONE_NAVBAR_Y      (CELLPHONE_VER_RES - CELLPHONE_NAVBAR_H)

#define CELLPHONE_ICONS_PER_PAGE (CELLPHONE_GRID_COLS * CELLPHONE_GRID_ROWS)

/*********************
 * THEME SYSTEM
 *********************/

/** Semantic color palette + font set for the cellphone demo.
 *
 * Gradient convention: each `<surface>` / `<surface>_grad` pair forms a
 * vertical gradient on that surface. The base slot is the conceptual color
 * (what the surface "looks like" at a glance); `_grad` is the second stop
 * that creates the depth tint. The renderer chooses which slot becomes the
 * top vs bottom stop based on the desired visual (e.g. statusbar darkens
 * upward toward the bezel, navbar lightens upward into the LCD). To render
 * a surface flat (no gradient), set `_grad` equal to its base -- this is
 * how the Dark theme suppresses gradients while sharing the same code path. */
typedef struct {
    lv_color_t primary;
    lv_color_t primary_dk;
    lv_color_t statusbar;
    lv_color_t statusbar_grad;
    lv_color_t navbar;
    lv_color_t navbar_grad;
    lv_color_t bg;
    lv_color_t bg_grad;
    lv_color_t card;
    lv_color_t text;
    lv_color_t text_sec;
    lv_color_t call_green;
    lv_color_t call_red;
    lv_color_t missed_red;
    lv_color_t sms_sent;
    lv_color_t sms_recv;
    lv_color_t indicator;

    const lv_font_t * font_sm;
    const lv_font_t * font_normal;
    const lv_font_t * font_heading;
    const lv_font_t * font_large;
    const lv_font_t * font_clock;
} cellphone_theme_t;

/**
 * Get the number of built-in themes.
 */
uint32_t cellphone_theme_count(void);

/**
 * Get a built-in theme by index (0 = Olive, 1 = Dark).
 */
const cellphone_theme_t * cellphone_theme_get(uint32_t idx);

/**
 * Get the currently active theme.
 */
const cellphone_theme_t * cellphone_theme_active(void);

/**
 * Get the index of the currently active theme.
 */
uint32_t cellphone_theme_active_index(void);

/**
 * Switch to a different theme by index.  Existing screens are NOT
 * restyled -- the new palette takes effect on subsequently created
 * widgets and on the next screen push/pop cycle.
 */
void cellphone_theme_set(uint32_t idx);

/* Convenience macros -- redirect through the active theme so every
 * existing source file picks up theme changes with zero edits. */
#define CELLPHONE_COLOR_PRIMARY        (cellphone_theme_active()->primary)
#define CELLPHONE_COLOR_PRIMARY_DK     (cellphone_theme_active()->primary_dk)
#define CELLPHONE_COLOR_STATUSBAR      (cellphone_theme_active()->statusbar)
#define CELLPHONE_COLOR_STATUSBAR_GRAD (cellphone_theme_active()->statusbar_grad)
#define CELLPHONE_COLOR_NAVBAR         (cellphone_theme_active()->navbar)
#define CELLPHONE_COLOR_NAVBAR_GRAD    (cellphone_theme_active()->navbar_grad)
#define CELLPHONE_COLOR_BG             (cellphone_theme_active()->bg)
#define CELLPHONE_COLOR_BG_GRAD        (cellphone_theme_active()->bg_grad)
#define CELLPHONE_COLOR_CARD         (cellphone_theme_active()->card)
#define CELLPHONE_COLOR_TEXT         (cellphone_theme_active()->text)
#define CELLPHONE_COLOR_TEXT_SEC     (cellphone_theme_active()->text_sec)
#define CELLPHONE_COLOR_CALL_GREEN   (cellphone_theme_active()->call_green)
#define CELLPHONE_COLOR_CALL_RED     (cellphone_theme_active()->call_red)
#define CELLPHONE_COLOR_MISSED_RED   (cellphone_theme_active()->missed_red)
#define CELLPHONE_COLOR_SMS_SENT     (cellphone_theme_active()->sms_sent)
#define CELLPHONE_COLOR_SMS_RECV     (cellphone_theme_active()->sms_recv)
#define CELLPHONE_COLOR_INDICATOR    (cellphone_theme_active()->indicator)

#define CELLPHONE_FONT_SM       (cellphone_theme_active()->font_sm)
#define CELLPHONE_FONT_NORMAL   (cellphone_theme_active()->font_normal)
#define CELLPHONE_FONT_HEADING  (cellphone_theme_active()->font_heading)
#define CELLPHONE_FONT_LARGE    (cellphone_theme_active()->font_large)
#define CELLPHONE_FONT_CLOCK    (cellphone_theme_active()->font_clock)

#ifndef LV_DEMO_CELLPHONE_FONT_CACHE_SM
#define LV_DEMO_CELLPHONE_FONT_CACHE_SM        1536
#endif

#ifndef LV_DEMO_CELLPHONE_FONT_CACHE_NORMAL
#define LV_DEMO_CELLPHONE_FONT_CACHE_NORMAL    1536
#endif

#ifndef LV_DEMO_CELLPHONE_FONT_CACHE_HEADING
#define LV_DEMO_CELLPHONE_FONT_CACHE_HEADING   0
#endif

#ifndef LV_DEMO_CELLPHONE_FONT_CACHE_LARGE
/* The home-icon plates render their glyphs inside LV_EVENT_DRAW_MAIN on
 * every paint tick, so the L2 cache must be on (the previous "0" budget
 * forced re-rasterization every frame). 1 KiB is the current floor that
 * still keeps the demo workload inside the harness' hit-rate window while
 * matching the MCU profile's SRAM budget. Larger values buy little on this
 * working set and push the full-run peak closer to the 76 KiB ceiling. */
#define LV_DEMO_CELLPHONE_FONT_CACHE_LARGE     1024
#endif

#ifndef LV_DEMO_CELLPHONE_FONT_CACHE_CLOCK
#define LV_DEMO_CELLPHONE_FONT_CACHE_CLOCK     0
#endif

#ifndef LV_DEMO_CELLPHONE_ICON_CACHE_22
/* The size-22 icon fallback font handles all 6 FA5 home-grid glyphs
 * painted in LV_EVENT_DRAW_MAIN every tick. The size-matched body
 * font's L2 cache doesn't help here because FA5 codepoints miss the
 * body charmap and fall through to this font. Steady-state working
 * set is ~540 B (6 glyphs * ~90 B/glyph A8); 768 B holds it with
 * a 1-glyph eviction buffer for transient screens (settings cog,
 * music note). The Test-11 gate (>=60%) reads 63.6% at both 1024
 * and 768 -- the gate's misses are dominated by SM/NORMAL text
 * caches, not this fallback, so trimming here is free. */
#define LV_DEMO_CELLPHONE_ICON_CACHE_22        768
#endif

/*********************
 * MOTION PRESETS
 *********************/

/** Named motion preset with asymmetric enter/exit timing. */
typedef struct {
    uint32_t enter_ms;
    uint32_t exit_ms;
    lv_anim_path_cb_t path_cb;
} cellphone_motion_preset_t;

extern const cellphone_motion_preset_t CELLPHONE_MOTION_STANDARD;   /* 240/220ms ease-in-out */
extern const cellphone_motion_preset_t CELLPHONE_MOTION_EMPHASIZED; /* 320/260ms ease-in-out */
extern const cellphone_motion_preset_t CELLPHONE_MOTION_QUICK;      /* 120/110ms ease-out */


/*********************
 * REAL-TIME CLOCK
 *********************/

/**
 * Format the current system time as "HH:MM" into buf.
 * @param buf      Output buffer (at least 6 bytes).
 * @param buf_len  Buffer size.
 */
void cellphone_time_now(char * buf, uint32_t buf_len);

/**
 * Format the current system date as "Weekday, Month DD" into buf.
 * @param buf      Output buffer (at least 32 bytes).
 * @param buf_len  Buffer size.
 */
void cellphone_date_now(char * buf, uint32_t buf_len);

/*********************
 * SCREEN STACK
 *********************/
#define CELLPHONE_SCREEN_STACK_DEPTH 8

typedef lv_obj_t * (*cellphone_screen_create_fn)(lv_obj_t * parent);

/**
 * Push a new screen onto the stack.  The create_fn is called to
 * populate the screen's content area.
 */
void cellphone_screen_push(cellphone_screen_create_fn fn);

/**
 * Pop the top screen and animate back to the previous one.
 */
void cellphone_screen_pop(void);

/**
 * Pop all screens and return to the home (bottom of stack).
 */
void cellphone_screen_home(void);

/**
 * Enable or disable push/pop/home transition animations.
 * Defaults to enabled. Test harnesses can disable transitions to avoid
 * transient double-screen redraw pressure during validation.
 */
void cellphone_screen_set_transitions_enabled(bool enabled);

/**
 * Get the current stack depth (0 = empty, 1 = home only).
 */
int cellphone_screen_depth(void);

/**
 * Get the current top-level page on the screen stack.
 * Returns NULL when the demo is not active.
 */
lv_obj_t * cellphone_screen_top(void);

/**
 * Check whether the top screen was created by the given factory.
 */
bool cellphone_screen_top_is(cellphone_screen_create_fn fn);

/**
 * Custom event code sent to a screen when it becomes the visible top screen.
 */
uint32_t cellphone_screen_event_shown(void);

/**
 * Custom event code sent to a screen when another screen covers it.
 */
uint32_t cellphone_screen_event_hidden(void);

/**
 * Show or hide the status bar and nav bar overlays.
 * The lock screen hides them so it can be full-screen.
 */
void cellphone_chrome_set_visible(bool visible);

/*********************
 * UI HELPERS
 *********************/

/**
 * Create an unstyled `lv_obj`: `lv_obj_create` + `lv_obj_remove_style_all`.
 * Caller is responsible for size, position, flags, and any further styling.
 */
lv_obj_t * cellphone_obj_bare(lv_obj_t * parent);

/**
 * Bare `lv_obj` filled with `color` at full opacity. Collapses the
 * `bare + bg_color + bg_opa COVER` pattern -- the most common surface
 * recipe in this demo. All bg props go on the same selector, so the
 * widget still costs exactly one local style[] entry.
 */
lv_obj_t * cellphone_obj_fill(lv_obj_t * parent, lv_color_t color);

/**
 * Bare `lv_obj` configured as a fully transparent surface with
 * borders disabled and the SCROLLABLE flag cleared. Collapses the
 * `bare + bg_opa=TRANSP + border=0 + clear SCROLLABLE` recipe that
 * CLAUDE.md flags as a known wart -- use this helper instead of
 * open-coding the four-call sequence on every screen.
 */
lv_obj_t * cellphone_obj_transparent(lv_obj_t * parent);

/**
 * Paint an existing object's main-part background with `color` at full
 * opacity. Same recipe as `cellphone_obj_fill` but for objects that
 * already exist (screen `parent`, list buttons, the demo root, etc.) --
 * collapses the two-line `bg_color + bg_opa COVER` pair scattered across
 * screens. Selector is 0 so widgets keep one local style[] entry.
 */
void cellphone_obj_paint_fill(lv_obj_t * obj, lv_color_t color);

/**
 * Bare `lv_obj` with a vertical gradient (`top` at top, `bot` at bottom)
 * at full opacity. Replaces the four-call gradient-bar recipe used by
 * statusbar / navbar / lock-screen panels.
 */
lv_obj_t * cellphone_obj_bar(lv_obj_t * parent, lv_color_t top, lv_color_t bot);

/**
 * Apply a vertical gradient (`top` at top, `bot` at bottom) at full opacity
 * to an existing object. The gradient-bar recipe shared with `cellphone_obj_bar`,
 * but for objects that were already created (e.g. `lv_obj_create` + a
 * `remove_style_all`) -- avoids re-creating the four-call sequence on screens.
 */
void cellphone_obj_paint_grad(lv_obj_t * obj, lv_color_t top, lv_color_t bot);

/**
 * Create a standard section header strip used across list/detail screens.
 * `title_align` controls how the title sits inside the bar (typically
 * LV_ALIGN_LEFT_MID or LV_ALIGN_CENTER). When `trailing_text` is non-NULL
 * and non-empty, a trailing label is added on the right.
 */
lv_obj_t * cellphone_section_header(lv_obj_t * parent, const char * title,
                                    lv_align_t title_align,
                                    const char * trailing_text);

/**
 * Create a label, set its text, and apply the given font + color.
 * Pass `NULL` for `text` to leave the label empty (filled in later by
 * the caller). `font` must be non-NULL.
 */
lv_obj_t * cellphone_label(lv_obj_t * parent, const char * text,
                           const lv_font_t * font, lv_color_t color);

/*********************
 * APP REGISTRY
 *********************/
/**
 * Procedural glyph drawer signature: paint a white silhouette inside
 * `plate` (the modern squircle) using `accent` for any color-mixed
 * highlights. Used for apps that don't have a bundled FA5 glyph --
 * keeps the home grid alphabet-free.
 */
typedef void (*cellphone_app_glyph_draw_fn)(lv_layer_t * layer,
                                            const lv_area_t * plate,
                                            lv_color_t accent);

typedef struct {
    const char * name;
    /* Exactly one of icon_text / glyph_draw is set. icon_text is an
     * FA5 symbol from the bundled cellphone-icons subset; glyph_draw is
     * a procedural drawer for apps without a bundled glyph. */
    const char * icon_text;
    cellphone_app_glyph_draw_fn glyph_draw;
    uint32_t icon_color_hex;
    cellphone_screen_create_fn create;
} cellphone_app_entry_t;

/**
 * Get the static app registry and its count.
 */
const cellphone_app_entry_t * cellphone_app_registry(uint32_t * count);

#ifdef __cplusplus
}
#endif

#endif /* LV_DEMO_CELLPHONE_COMMON_H */
