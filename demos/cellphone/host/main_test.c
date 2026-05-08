/**
 * @file main_test.c
 *
 * Automated test harness for the Cell Phone Demo.
 * Validates the full user journey:
 *   boot -> unlock -> home -> launch each app -> back -> repeat
 * Gates on screen stack depth, object tree content, pool-growth
 * deltas, and per-font cache budgets.  Pure structural validation;
 * no image capture (the SDL host is the visual-inspection path).
 *
 * Usage:
 *   build/cellphone_test [--focused]
 */

#include "lvgl.h"
#include "demos/lv_demos.h"
#include "demos/cellphone/lv_demo_cellphone_common.h"
#include "demos/cellphone/lv_demo_cellphone_home.h"
#include "demos/cellphone/lv_demo_cellphone_lock.h"
#include "demos/cellphone/lv_demo_cellphone_dialer.h"
#include "demos/cellphone/lv_demo_cellphone_calc.h"
#include "demos/cellphone/lv_demo_cellphone_contacts.h"
#include "demos/cellphone/lv_demo_cellphone_data.h"
#include "demos/cellphone/lv_demo_cellphone_navbar.h"
#include "demos/cellphone/lv_demo_cellphone_sms.h"
#include "demos/cellphone/lv_demo_cellphone_music.h"
#include "demos/cellphone/lv_demo_cellphone_photo.h"
#include "demos/cellphone/lv_demo_cellphone_settings.h"
#include "demos/cellphone/lv_demo_cellphone_game.h"
#include "demos/cellphone/lv_demo_cellphone_calllog.h"
#include "demos/cellphone/mem_report.h"
#if defined(LV_DEMO_CELLPHONE_SKIN) && LV_DEMO_CELLPHONE_SKIN && LV_USE_SDL
    #include "demos/cellphone/lv_demo_cellphone_skin.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*********************
 *      DEFINES
 *********************/
#define SCR_W   CELLPHONE_HOR_RES    /* 240 */
#define SCR_H   CELLPHONE_VER_RES    /* 320 */

/**********************
 *  STATIC VARIABLES
 **********************/
static lv_display_t * s_disp;
static lv_indev_t  *  s_indev;
static int s_pass;
static int s_fail;
static bool s_focused_mode;

/* Virtual touch state driven by sim_* helpers */
static lv_indev_state_t s_touch_state = LV_INDEV_STATE_RELEASED;
static lv_point_t       s_touch_point = {0, 0};

/* Snapshot of L2 cache counters at the start of Test 11. The
 * vector-font-cache phase computes a delta against these to gate on
 * Test-11-local hit rate rather than the process-lifetime sum. */
static uint32_t s_l2_hits_pre_test11;
static uint32_t s_l2_misses_pre_test11;

/* Per-test peak attribution. The allocator's `max_used` is a process-
 * lifetime monotonic high-water mark, so per-test deltas against it
 * collapse to zero after the first peak and a regression in any single
 * test can hide behind another test's slack. We instead sample the
 * live `used` value after every simulated frame (every lv_timer_handler
 * tick that sim_wait runs) and track the per-test max here. test_peak_*
 * helpers drive it; sim_wait is what samples. */
static size_t s_test_peak_used;

/**********************
 *  TOUCH SIMULATION
 **********************/

static void touch_read_cb(lv_indev_t * indev, lv_indev_data_t * data)
{
    LV_UNUSED(indev);
    data->point = s_touch_point;
    data->state = s_touch_state;
}

static void sim_set_touch_point(int32_t x, int32_t y)
{
#if defined(LV_DEMO_CELLPHONE_SKIN) && LV_DEMO_CELLPHONE_SKIN && LV_USE_SDL
    const cellphone_skin_t * skin = cellphone_skin_active();
    if(skin) {
        x += skin->lcd_x;
        y += skin->lcd_y;
    }
#endif

    s_touch_point.x = x;
    s_touch_point.y = y;
}

/** Read live (steady-state) pool occupancy.  Used by the per-test peak
 *  sampler -- max_used is process-lifetime monotonic, so deltas across
 *  test boundaries collapse to zero after the first peak. */
static size_t mem_used_now(void)
{
    lv_mem_monitor_t m;
    lv_mem_monitor(&m);
    return m.free_size <= m.total_size ? (size_t)(m.total_size - m.free_size) : 0;
}

/** Sample current pool occupancy and bump the per-test peak. */
static void test_peak_sample(void)
{
    size_t used = mem_used_now();
    if(used > s_test_peak_used) s_test_peak_used = used;
}

/** Reset the per-test peak. Sampled once at reset so the captured peak
 *  reflects state-on-entry, not just mid-test transients. */
static void test_peak_reset(void)
{
    s_test_peak_used = 0;
    test_peak_sample();
}

static size_t test_peak_get(void)
{
    return s_test_peak_used;
}

/** Advance LVGL by ms milliseconds. */
static void sim_wait(uint32_t ms)
{
    uint32_t frames = ms / 16;
    if(frames < 1) frames = 1;
    for(uint32_t i = 0; i < frames; i++) {
        lv_tick_inc(16);
        lv_timer_handler();
        test_peak_sample();
    }
}

/** Read the allocator-tracked peak. Captures every alloc/free along
 *  the path, including transients within a single lv_timer_handler()
 *  call that frame-end polling cannot see. */
static size_t mem_peak_used(void)
{
    lv_mem_monitor_t m;
    lv_mem_monitor(&m);
    return m.max_used;
}

static void print_mem_stats(const char * tag)
{
    lv_mem_monitor_t m;
    lv_mem_monitor(&m);

    size_t used = m.free_size <= m.total_size
                  ? (size_t)(m.total_size - m.free_size)
                  : 0;

    printf("  [mem] %s: used=%zu free=%zu frag=%" LV_PRIu32
           " max=%zu KiB total=%zu\n",
           tag, used, m.free_size, m.frag_pct, mem_peak_used() / 1024,
           m.total_size);
}

static void print_screen_state(const char * tag)
{
    printf("  [state] %s: depth=%d top=%p home=%d dialer=%d calc=%d\n",
           tag,
           cellphone_screen_depth(),
           (void *)cellphone_screen_top(),
           cellphone_screen_top_is(cellphone_home_create),
           cellphone_screen_top_is(cellphone_dialer_create),
           cellphone_screen_top_is(cellphone_calc_create));
}

static void sim_wait_trace(const char * tag, uint32_t ms)
{
    uint32_t frames = ms / 16;
    if(frames < 1) frames = 1;

    printf("  [trace] wait start: %s (%" LV_PRIu32 " ms, %" LV_PRIu32 " frames)\n",
           tag, ms, frames);
    print_screen_state(tag);
    print_mem_stats(tag);

    for(uint32_t i = 0; i < frames; i++) {
        char frame_tag[96];
        lv_snprintf(frame_tag, sizeof(frame_tag), "%s frame %" LV_PRIu32 "/%" LV_PRIu32,
                    tag, i + 1, frames);
        printf("  [trace] %s begin\n", frame_tag);
        lv_tick_inc(16);
        lv_timer_handler();
        printf("  [trace] %s end\n", frame_tag);
        print_screen_state(frame_tag);
        print_mem_stats(frame_tag);
    }

    printf("  [trace] wait done: %s\n", tag);
}

/** Press at (x, y) and hold. */
static void sim_press(int32_t x, int32_t y)
{
    sim_set_touch_point(x, y);
    s_touch_state = LV_INDEV_STATE_PRESSED;
    sim_wait(32);
}

/** Release the touch. */
static void sim_release(void)
{
    s_touch_state = LV_INDEV_STATE_RELEASED;
    sim_wait(32);
}

/** Tap at (x, y): press then release. */
static void sim_click(int32_t x, int32_t y)
{
    sim_press(x, y);
    sim_release();
}

/** Drag from (x1,y1) to (x2,y2) over duration_ms. */
static void sim_drag(int32_t x1, int32_t y1, int32_t x2, int32_t y2,
                     uint32_t duration_ms)
{
    uint32_t steps = duration_ms / 16;
    if(steps < 2) steps = 2;
    sim_press(x1, y1);
    for(uint32_t i = 1; i <= steps; i++) {
        sim_set_touch_point(x1 + (x2 - x1) * (int32_t)i / (int32_t)steps,
                            y1 + (y2 - y1) * (int32_t)i / (int32_t)steps);
        sim_wait(16);
    }
    sim_release();
}

static bool wait_until_depth(int expected_depth, uint32_t timeout_ms)
{
    uint32_t elapsed = 0;
    while(elapsed <= timeout_ms) {
        if(cellphone_screen_depth() == expected_depth) return true;
        sim_wait(16);
        elapsed += 16;
    }

    return cellphone_screen_depth() == expected_depth;
}

static bool wait_until_top(cellphone_screen_create_fn fn, uint32_t timeout_ms)
{
    uint32_t elapsed = 0;
    while(elapsed <= timeout_ms) {
        if(cellphone_screen_top_is(fn)) return true;
        sim_wait(16);
        elapsed += 16;
    }

    return cellphone_screen_top_is(fn);
}

/**********************
 *  CHECK
 **********************/

static void check(const char * name, bool condition)
{
    if(condition) {
        printf("  PASS: %s\n", name);
        s_pass++;
    }
    else {
        printf("  FAIL: %s\n", name);
        s_fail++;
    }
}

/** Count total objects in the tree rooted at obj (recursive). */
static uint32_t obj_tree_count(lv_obj_t * obj)
{
    uint32_t n = 1;
    uint32_t cc = lv_obj_get_child_count(obj);
    for(uint32_t i = 0; i < cc; i++) {
        n += obj_tree_count(lv_obj_get_child(obj, i));
    }
    return n;
}

/** True if any direct-child label of @p obj equals @p needle exactly,
 *  matches it after a leading FA-glyph + space (e.g. the Call/End-Call
 *  buttons whose label text is `LV_SYMBOL_CALL " Call"`), or matches
 *  it before a trailing space-suffix.  Substring match is too loose:
 *  the calc keypad has both "+/-" and "/", so strstr("/") would hit
 *  the wrong button. */
static bool obj_has_child_label_text(lv_obj_t * obj, const char * needle)
{
    size_t nl = strlen(needle);
    uint32_t cc = lv_obj_get_child_count(obj);
    for(uint32_t i = 0; i < cc; i++) {
        lv_obj_t * c = lv_obj_get_child(obj, i);
        if(!lv_obj_check_type(c, &lv_label_class)) continue;
        const char * t = lv_label_get_text(c);
        if(!t) continue;
        if(strcmp(t, needle) == 0) return true;
        size_t tl = strlen(t);
        if(tl > nl + 1 && strcmp(t + tl - nl, needle) == 0
           && t[tl - nl - 1] == ' ') return true;
        if(tl > nl + 1 && strncmp(t, needle, nl) == 0 && t[nl] == ' ') return true;
    }
    return false;
}

/** Find the first clickable widget in the @p root subtree whose direct-
 *  child label contains @p needle. Returns NULL when not found. */
static lv_obj_t * find_clickable_with_label(lv_obj_t * root, const char * needle)
{
    if(!root) return NULL;
    if(lv_obj_has_flag(root, LV_OBJ_FLAG_CLICKABLE)
       && obj_has_child_label_text(root, needle)) {
        return root;
    }
    uint32_t cc = lv_obj_get_child_count(root);
    for(uint32_t i = 0; i < cc; i++) {
        lv_obj_t * f = find_clickable_with_label(lv_obj_get_child(root, i), needle);
        if(f) return f;
    }
    return NULL;
}

/** True if any label anywhere under @p root contains @p needle. */
static bool screen_has_label_text(lv_obj_t * root, const char * needle)
{
    if(!root) return false;
    if(lv_obj_check_type(root, &lv_label_class)) {
        const char * t = lv_label_get_text(root);
        if(t && strstr(t, needle)) return true;
    }
    uint32_t cc = lv_obj_get_child_count(root);
    for(uint32_t i = 0; i < cc; i++) {
        if(screen_has_label_text(lv_obj_get_child(root, i), needle)) return true;
    }
    return false;
}

/** True if any non-button label anywhere under @p root equals @p text
 *  exactly.  A "non-button label" is one whose immediate parent is not
 *  an `lv_button` -- this excludes keypad button captions (e.g. the
 *  calculator's "0", "1", ... or "C" buttons) so a check for the
 *  display value isn't satisfied by an unrelated button caption.
 *
 *  Direct-child-only matching cannot be used here because the screen
 *  stack wraps each page in an outer scr -> content container, leaving
 *  the display label two levels below the page returned by
 *  `cellphone_screen_top()`. */
static bool screen_has_direct_label_text(lv_obj_t * root, const char * text)
{
    if(!root) return false;
    if(lv_obj_check_type(root, &lv_label_class)) {
        lv_obj_t * parent = lv_obj_get_parent(root);
        bool inside_button = parent && lv_obj_check_type(parent, &lv_button_class);
        if(!inside_button) {
            const char * label = lv_label_get_text(root);
            if(label && strcmp(label, text) == 0) return true;
        }
    }
    uint32_t cc = lv_obj_get_child_count(root);
    for(uint32_t i = 0; i < cc; i++) {
        if(screen_has_direct_label_text(lv_obj_get_child(root, i), text)) return true;
    }
    return false;
}

/** Find the first descendant of @p root with class @p cls. */
static lv_obj_t * find_widget_by_class(lv_obj_t * root, const lv_obj_class_t * cls)
{
    if(!root) return NULL;
    if(lv_obj_check_type(root, cls)) return root;
    uint32_t cc = lv_obj_get_child_count(root);
    for(uint32_t i = 0; i < cc; i++) {
        lv_obj_t * f = find_widget_by_class(lv_obj_get_child(root, i), cls);
        if(f) return f;
    }
    return NULL;
}

static lv_obj_t * find_first_scrollable_descendant(lv_obj_t * root)
{
    if(!root) return NULL;
    if(lv_obj_has_flag(root, LV_OBJ_FLAG_SCROLLABLE) &&
       lv_obj_get_scroll_dir(root) != LV_DIR_NONE) {
        return root;
    }

    uint32_t cc = lv_obj_get_child_count(root);
    for(uint32_t i = 0; i < cc; i++) {
        lv_obj_t * f = find_first_scrollable_descendant(lv_obj_get_child(root, i));
        if(f) return f;
    }

    return NULL;
}

/** Convert display-global coordinates (what lv_obj_get_coords returns)
 *  to LCD-local (what sim_set_touch_point expects).  When the bezel skin
 *  is active the SDL window is larger than the LCD, and sim_set_touch_point
 *  re-applies the offset; without this conversion any obj-coord-driven
 *  tap lands off-screen. */
static void lcd_local_from_global(int32_t * x, int32_t * y)
{
#if defined(LV_DEMO_CELLPHONE_SKIN) && LV_DEMO_CELLPHONE_SKIN && LV_USE_SDL
    const cellphone_skin_t * skin = cellphone_skin_active();
    if(skin) {
        *x -= skin->lcd_x;
        *y -= skin->lcd_y;
    }
#endif
}

/** Dispatch CLICKED to the first clickable widget on the active screen
 *  whose direct-child label matches @p needle.  Returns false when no
 *  match.  Bypasses the indev so back-to-back keypad taps don't trip
 *  LVGL's scroll-detection heuristic on the inter-button touch jump --
 *  what we want to gate on here is the widget's CLICKED handler
 *  (calculator arithmetic, dialer keypad accumulation, etc.), not the
 *  indev gesture path itself.  The lock-screen slide and any other
 *  gesture test continues to exercise the indev via sim_drag. */
static bool sim_tap_label(const char * needle)
{
    lv_obj_t * top = cellphone_screen_top();
    lv_obj_t * btn = find_clickable_with_label(top, needle);
    if(!btn) return false;
    lv_obj_send_event(btn, LV_EVENT_CLICKED, NULL);
    return true;
}

/** Real-indev companion to sim_tap_label.  Same target-discovery logic
 *  but drives the physical lv_indev_* path -- lv_indev_reset to drop the
 *  previous gesture's act_obj/scroll_obj, park the touch at the target
 *  with state=RELEASED for one frame so LVGL's `last_point` updates,
 *  then a real PRESSED/RELEASED click pair.  Without the parking trick
 *  the next PRESSED's vect (target - last_point from a prior tap on a
 *  different widget) trips find_scroll_obj on any surrounding scrollable
 *  page and the click is reclassified as a scroll.  The keypad-CLICKED
 *  handlers behave identically under this path on a desktop, but a
 *  regression that moves the click target inside a scrollable container
 *  or that breaks indev hit-test ordering would slip past sim_tap_label
 *  while showing up here. */
static bool sim_tap_label_indev(const char * needle)
{
    lv_obj_t * top = cellphone_screen_top();
    lv_obj_t * btn = find_clickable_with_label(top, needle);
    if(!btn) return false;

    lv_area_t a;
    lv_obj_get_coords(btn, &a);
    int32_t cx = (a.x1 + a.x2) / 2;
    int32_t cy = (a.y1 + a.y2) / 2;
    lcd_local_from_global(&cx, &cy);

    lv_indev_reset(s_indev, NULL);
    sim_set_touch_point(cx, cy);
    sim_wait(32);
    sim_click(cx, cy);
    return true;
}

/** Find the first label widget anywhere under @p root whose text equals
 *  @p text exactly. Used by gesture tests that need a stable handle on
 *  a chrome label (e.g. the photo viewer's title bar) to read its
 *  parent's geometry without hard-coding screen offsets. */
static lv_obj_t * find_label_obj(lv_obj_t * root, const char * text)
{
    if(!root) return NULL;
    if(lv_obj_check_type(root, &lv_label_class)) {
        const char * t = lv_label_get_text(root);
        if(t && strcmp(t, text) == 0) return root;
    }
    uint32_t cc = lv_obj_get_child_count(root);
    for(uint32_t i = 0; i < cc; i++) {
        lv_obj_t * f = find_label_obj(lv_obj_get_child(root, i), text);
        if(f) return f;
    }
    return NULL;
}

static lv_obj_t * find_label_prefix_obj(lv_obj_t * root, const char * prefix)
{
    if(!root) return NULL;
    if(lv_obj_check_type(root, &lv_label_class)) {
        const char * t = lv_label_get_text(root);
        if(t && strncmp(t, prefix, strlen(prefix)) == 0) return root;
    }
    uint32_t cc = lv_obj_get_child_count(root);
    for(uint32_t i = 0; i < cc; i++) {
        lv_obj_t * f = find_label_prefix_obj(lv_obj_get_child(root, i), prefix);
        if(f) return f;
    }
    return NULL;
}

/** Walk @p root and check that every list-button under it paints with
 *  @p card.  Used by the per-theme list-button color gate.  Stores the
 *  pass / fail count via out-pointers so the caller can emit a single
 *  aggregated check() with a useful failure message. */
static void list_buttons_check_card(lv_obj_t * root, lv_color_t card,
                                    uint32_t * total, uint32_t * mismatched)
{
    if(!root) return;
    if(lv_obj_check_type(root, &lv_list_button_class)) {
        (*total)++;
        lv_color_t c = lv_obj_get_style_bg_color(root, 0);
        if(!lv_color_eq(c, card))(*mismatched)++;
    }
    uint32_t cc = lv_obj_get_child_count(root);
    for(uint32_t i = 0; i < cc; i++) {
        list_buttons_check_card(lv_obj_get_child(root, i), card, total, mismatched);
    }
}

/** Walk @p root and check every direct calllog row -- those that are
 *  CLICKABLE flex-row containers under a vertically-scrolling list --
 *  paints with @p card.  Calllog rows aren't lv_list_buttons, they are
 *  cellphone_obj_fill containers (see lv_demo_cellphone_calllog.c
 *  row_create), so the lv_list_button_class walker would miss them.
 *  Instead, identify them as direct children of a SCROLLABLE LV_DIR_VER
 *  container that have CLICKABLE set and a positive height -- the
 *  shape that calllog row_create produces. */
static void calllog_rows_check_card(lv_obj_t * root, lv_color_t card,
                                    uint32_t * total, uint32_t * mismatched)
{
    if(!root) return;
    bool is_scroll_list = lv_obj_has_flag(root, LV_OBJ_FLAG_SCROLLABLE)
                          && (lv_obj_get_scroll_dir(root) & LV_DIR_VER);
    if(is_scroll_list) {
        uint32_t cc = lv_obj_get_child_count(root);
        for(uint32_t i = 0; i < cc; i++) {
            lv_obj_t * c = lv_obj_get_child(root, i);
            if(lv_obj_has_flag(c, LV_OBJ_FLAG_CLICKABLE)) {
                (*total)++;
                lv_color_t bg = lv_obj_get_style_bg_color(c, 0);
                if(!lv_color_eq(bg, card))(*mismatched)++;
            }
        }
        return;
    }
    uint32_t cc = lv_obj_get_child_count(root);
    for(uint32_t i = 0; i < cc; i++) {
        calllog_rows_check_card(lv_obj_get_child(root, i), card, total, mismatched);
    }
}

static bool navbar_tab_click_or_fail(uint32_t index, const char * name)
{
    bool ok = cellphone_navbar_tab_click(index);
    check(name, ok);
    return ok;
}

static bool navbar_tab_tap_or_fail(uint32_t index, const char * name)
{
    if(index >= 3) {
        check(name, false);
        return false;
    }

    int32_t tab_w = CELLPHONE_HOR_RES / 3;
    int32_t x = tab_w * (int32_t)index + tab_w / 2;
    int32_t y = CELLPHONE_NAVBAR_Y + CELLPHONE_NAVBAR_H / 2;
    sim_click(x, y);
    bool ok = true;
    check(name, ok);
    return ok;
}

static bool navbar_tab_tap_wait_or_fail(uint32_t index, const char * tap_name,
                                        cellphone_screen_create_fn expected_top,
                                        int expected_depth, const char * state_name)
{
    if(!navbar_tab_tap_or_fail(index, tap_name)) return false;

    bool ok = true;
    if(expected_top) {
        ok = wait_until_top(expected_top, 1000);
    }
    else {
        ok = wait_until_depth(expected_depth, 1000);
    }
    check(state_name, ok);
    return ok;
}

static bool navbar_tab_click_wait_or_fail(uint32_t index, const char * click_name,
                                          cellphone_screen_create_fn expected_top,
                                          int expected_depth, const char * state_name)
{
    if(!navbar_tab_click_or_fail(index, click_name)) return false;

    bool ok = true;
    if(expected_top) {
        ok = wait_until_top(expected_top, 1000);
    }
    if(ok && expected_depth >= 0) {
        ok = wait_until_depth(expected_depth, 1000);
    }

    check(state_name, ok);
    return ok;
}

static void test_dialer_keypad(void)
{
    printf("\n--- Test: Phone keypad interaction ---\n");
    test_peak_reset();

    printf("  [progress] dialer keypad home reset start\n");
    cellphone_screen_home();
    sim_wait(500);
    printf("  [progress] dialer keypad home reset done\n");

    int depth_before = cellphone_screen_depth();
    printf("  [progress] dialer keypad push start\n");
    cellphone_screen_push(cellphone_dialer_create);
    printf("  [progress] dialer keypad push done\n");
    sim_wait(500);
    printf("  [progress] dialer keypad settle done\n");

    check("dialer pushed for keypad test", cellphone_screen_depth() == depth_before + 1);
    check("dialer on top", cellphone_screen_top_is(cellphone_dialer_create));

    /* Drive the full call flow rather than asserting just one tap.
     * Type 1-2-3, place the call, wait for the in-call timer's first
     * tick, then end the call.  This exercises s_number accumulation,
     * the call_cb -> incall_create push, the elapsed-time timer
     * (1 s cadence), and the end-call pop -- all paths that an MCU
     * port of LVGL timers / animations is most likely to break. */
    lv_obj_t * dialer_top = cellphone_screen_top();

    check("tap '1' digit",            sim_tap_label("1"));
    sim_wait(32);
    check("tap '2' digit",            sim_tap_label("2"));
    sim_wait(32);
    check("tap '3' digit",            sim_tap_label("3"));
    sim_wait(32);
    check("dialer number shows '123'",
          screen_has_label_text(dialer_top, "123"));

    int depth_pre_call = cellphone_screen_depth();
    check("tap Call",                 sim_tap_label("Call"));
    sim_wait(200);
    check("in-call screen pushed",    cellphone_screen_depth() == depth_pre_call + 1);
    lv_obj_t * incall = cellphone_screen_top();
    check("in-call status 'Calling...' rendered",
          screen_has_label_text(incall, "Calling"));
    check("in-call shows dialed number 123",
          screen_has_label_text(incall, "123"));
    check("in-call timer label starts at 00:00",
          screen_has_label_text(incall, "00:00"));

    /* Timer fires at 1000 ms cadence in incall_create.  Advance just
     * past one tick and assert the label rolled over to "00:01" so a
     * broken lv_timer in a target build is caught in seconds, not
     * minutes-of-watching. */
    sim_wait(1100);
    check("in-call timer flipped to 00:01",
          screen_has_label_text(incall, "00:01"));

    check("tap End Call",             sim_tap_label("End Call"));
    sim_wait(200);
    check("in-call screen popped",    cellphone_screen_depth() == depth_pre_call);
    check("returned to dialer",
          cellphone_screen_top_is(cellphone_dialer_create));

    printf("  [progress] dialer keypad pop start\n");
    cellphone_screen_pop();
    printf("  [progress] dialer keypad pop done\n");
    sim_wait(1000);
    printf("  [progress] dialer keypad post-pop settle done\n");
    check("dialer keypad test returned home", cellphone_screen_depth() == depth_before);
    printf("  [peak] dialer keypad test peak: %zu KiB\n", test_peak_get() / 1024);
}

static void test_calc_keypad(void)
{
    printf("\n--- Test: Calculator keypad interaction ---\n");
    test_peak_reset();

    printf("  [progress] calc keypad home reset start\n");
    cellphone_screen_home();
    sim_wait(500);
    printf("  [progress] calc keypad home reset done\n");
    print_screen_state("calc after home reset");
    print_mem_stats("calc after home reset");

    int depth_before = cellphone_screen_depth();
    printf("  [progress] calc keypad push start\n");
    print_mem_stats("calc before push");
    cellphone_screen_push(cellphone_calc_create);
    printf("  [progress] calc keypad push done\n");
    print_screen_state("calc after push");
    print_mem_stats("calc after push");
    sim_wait_trace("calc settle", 500);
    printf("  [progress] calc keypad settle done\n");

    check("calculator pushed for keypad test", cellphone_screen_depth() == depth_before + 1);
    check("calculator on top", cellphone_screen_top_is(cellphone_calc_create));

    /* Arithmetic correctness path.  The structural tap above only proved
     * the screen survives one click.  Drive a real expression through
     * execute_op + format_scaled and assert the rendered display, plus
     * the divide-by-zero error latch and clear path -- those are the
     * three branches an MCU integrator is most likely to break when
     * porting calc to a smaller int width or a different scaling.
     *
     * sim_tap_label dispatches CLICKED via lv_obj_send_event rather than
     * the indev, so we need only a single-frame settle for the static
     * label to refresh. */
    lv_obj_t * calc_top = cellphone_screen_top();

    const uint32_t TAP_GAP = 32;

    /* First digit + operator land via the real indev path so a regression
     * that breaks click-through on a physical touch panel (gesture
     * misclassification, hit-test ordering, button-matrix scroll
     * heuristic) trips a structural gate, not just a manual demo run.
     * The remaining keypad assertions stay on sim_tap_label so the
     * arithmetic/error paths are isolated from indev gesture variance. */
    check("tap '1' (indev)", sim_tap_label_indev("1"));
    sim_wait(TAP_GAP);
    check("tap '2'", sim_tap_label("2"));
    sim_wait(TAP_GAP);
    check("display shows '12' after digits",
          screen_has_label_text(calc_top, "12"));

    check("tap '+' (indev)", sim_tap_label_indev("+"));
    sim_wait(TAP_GAP);
    check("tap '7'", sim_tap_label("7"));
    sim_wait(TAP_GAP);
    check("tap '='", sim_tap_label("="));
    sim_wait(TAP_GAP);
    check("display shows '19' after 12+7=",
          screen_has_label_text(calc_top, "19"));

    check("tap 'C' to clear",     sim_tap_label("C"));
    sim_wait(TAP_GAP);
    check("display shows '0' after clear",
          screen_has_direct_label_text(calc_top, "0"));

    /* Divide-by-zero must latch s_error and render "Error" via
     * update_display(); subsequent digit/op taps are ignored until C. */
    check("tap '5'", sim_tap_label("5"));
    sim_wait(TAP_GAP);
    check("tap '/'", sim_tap_label("/"));
    sim_wait(TAP_GAP);
    check("tap '0'", sim_tap_label("0"));
    sim_wait(TAP_GAP);
    check("tap '=' for 5/0",      sim_tap_label("="));
    sim_wait(TAP_GAP);
    check("display shows 'Error' after divide-by-zero",
          screen_has_direct_label_text(calc_top, "Error"));

    check("tap 'C' clears error", sim_tap_label("C"));
    sim_wait(TAP_GAP);
    check("display shows '0' after error clear",
          screen_has_direct_label_text(calc_top, "0"));
    check("error label cleared",
          !screen_has_direct_label_text(calc_top, "Error"));

    printf("  [progress] calc keypad pop start\n");
    cellphone_screen_pop();
    printf("  [progress] calc keypad pop done\n");
    sim_wait(1000);
    printf("  [progress] calc keypad post-pop settle done\n");
    check("calculator keypad test returned home", cellphone_screen_depth() == depth_before);
    printf("  [peak] calc keypad test peak: %zu KiB\n", test_peak_get() / 1024);
}

/**********************
 *       TESTS
 **********************/

static void test_01_boot(void)
{
    printf("\n--- Test 01: Boot (lock screen) ---\n");
    lv_demo_cellphone();
    sim_wait(500);

    check("demo started", cellphone_screen_depth() == 2);
    check("booted to lock screen", cellphone_screen_top_is(cellphone_lock_create));
}

/** Drive the lock-screen slider via simulated drag gestures and assert
 *  threshold behavior on both sides of the 90% cutoff.  The previous
 *  shape of this test bypassed the gesture entirely (chrome_set_visible
 *  + screen_pop), which left slider physics, the reset-on-release path,
 *  and the duplicate-VALUE_CHANGED latch all untested -- exactly the
 *  paths that diverge between desktop and a real touch panel.  Use
 *  find_widget_by_class to locate the slider rather than hard-coding
 *  screen y, so a bezel skin or layout tweak can't silently miss the
 *  drag target. */
static void test_02_unlock(void)
{
    printf("\n--- Test 02: Slide to unlock ---\n");

    lv_obj_t * lock = cellphone_screen_top();
    lv_obj_t * slider = find_widget_by_class(lock, &lv_slider_class);
    check("lock-screen slider present", slider != NULL);
    if(!slider) return;

    lv_area_t s_area;
    lv_obj_get_coords(slider, &s_area);
    int32_t x1 = s_area.x1, x2 = s_area.x2;
    int32_t y1 = s_area.y1, y2 = s_area.y2;
    /* Slider coords come back in display-global space (skin offset
     * already applied).  sim_set_touch_point re-applies the same offset,
     * so feed it LCD-local. */
    lcd_local_from_global(&x1, &y1);
    lcd_local_from_global(&x2, &y2);
    int32_t y = (y1 + y2) / 2;
    int32_t x_start = x1 + 6;            /* knob origin, value=0 */
    int32_t x_mid   = (x1 + x2) / 2;     /* ~50% */
    int32_t x_far   = x2 - 6;            /* past the 90% threshold */

    /* Under-threshold drag: slider value crosses ~50%, release fires the
     * RELEASED branch, and the reset animation snaps the value back to 0
     * without ever crossing 90.  Lock screen must stay on top. */
    int depth_locked = cellphone_screen_depth();
    sim_drag(x_start, y, x_mid, y, 200);
    sim_wait(400);  /* slider reset anim is 200 ms; leave headroom */

    check("under-threshold drag: depth unchanged",
          cellphone_screen_depth() == depth_locked);
    check("under-threshold drag: lock stays on top",
          cellphone_screen_top_is(cellphone_lock_create));
    check("under-threshold drag: slider snapped back to 0",
          lv_slider_get_value(slider) == 0);

    /* Full slide: drag past 90% to trigger the shutter reveal.  The
     * shutter callback chains lv_anim (200 ms top + 200/100 ms bottom +
     * delete delay), so wait long enough for the pop to settle. */
    sim_drag(x_start, y, x_far, y, 250);
    sim_wait(800);

    check("full slide: depth dropped one level",
          cellphone_screen_depth() == depth_locked - 1);
    check("full slide: lock screen removed",
          !cellphone_screen_top_is(cellphone_lock_create));
    check("full slide: home screen on top",
          cellphone_screen_top_is(cellphone_home_create));
}

static void test_app(const char * name, cellphone_screen_create_fn fn)
{
    printf("\n--- Test: %s ---\n", name);
    test_peak_reset();

    int depth_before = cellphone_screen_depth();

    /* Push the app screen directly */
    printf("  [progress] push start: %s\n", name);
    cellphone_screen_push(fn);
    printf("  [progress] push done: %s\n", name);
    sim_wait(500);
    printf("  [progress] settle done: %s\n", name);

    check("pushed screen", cellphone_screen_depth() == depth_before + 1);

    /* Verify the app screen has meaningful content */
    lv_obj_t * scr = cellphone_screen_top();
    check("top screen exists", scr != NULL);
    if(scr) {
        printf("  [progress] tree count start: %s\n", name);
        uint32_t tree = obj_tree_count(scr);
        printf("  [progress] tree count done: %s\n", name);
        check("screen has content", tree > 2);
        printf("  (object tree: %" LV_PRIu32 " nodes)\n", tree);
    }

    /* Pop back */
    printf("  [progress] pop start: %s\n", name);
    cellphone_screen_pop();
    printf("  [progress] pop done: %s\n", name);
    sim_wait(1000);
    printf("  [progress] post-pop settle done: %s\n", name);

    check("popped back", cellphone_screen_depth() == depth_before);
    printf("  [peak] %s test peak: %zu KiB\n", name, test_peak_get() / 1024);
}

/** Photos coverage with gestures: thumbnail tap pushes the viewer,
 *  fullscreen tap toggles chrome via animation (verified by reading the
 *  title bar's y), and a horizontal indev drag must advance the
 *  tileview to the next photo.  test_app("Photos", ...) only smoke-
 *  tested push/pop -- gestures are the path most likely to diverge on
 *  a touch MCU. */
static void test_photos_gestures(void)
{
    printf("\n--- Test: Photos (with gestures) ---\n");
    test_peak_reset();
    int depth_before = cellphone_screen_depth();

    cellphone_screen_push(cellphone_photo_create);
    sim_wait(500);
    check("Photos pushed", cellphone_screen_depth() == depth_before + 1);

    /* Tap thumbnail '1' (label child of the first grid cell, CLICKABLE) */
    int depth_after_thumb = cellphone_screen_depth();
    check("tap thumbnail '1'", sim_tap_label("1"));
    sim_wait(500);
    check("photo viewer pushed",
          cellphone_screen_depth() == depth_after_thumb + 1);

    /* Resolve the viewer's title bar by walking up from its 'Photos'
     * label child.  The bar's y starts at 0 and the fullscreen toggle
     * animates it to -28 (and back).  Reading y is more precise than
     * inferring visibility from labels, since the bar still exists in
     * the tree after the slide. */
    lv_obj_t * viewer = cellphone_screen_top();
    lv_obj_t * title_lbl = find_label_obj(viewer, "Photos");
    check("viewer title 'Photos' label present", title_lbl != NULL);
    if(title_lbl) {
        lv_obj_t * title_bar = lv_obj_get_parent(title_lbl);
        check("viewer title bar starts at y=0", lv_obj_get_y(title_bar) == 0);

        /* Enter fullscreen: viewer_tap_cb animates over 400 ms, settle 500.
         * sim_tap_label dispatches CLICKED on the rect (its child label is
         * 'Photo 1'); the rect's CLICKED handler is the toggle. */
        check("tap photo to enter fullscreen", sim_tap_label("Photo 1"));
        sim_wait(500);
        check("title bar slid off-screen at y=-28",
              lv_obj_get_y(title_bar) == -28);

        /* Exit fullscreen: 300 ms anim, settle 500. */
        check("tap photo to exit fullscreen", sim_tap_label("Photo 1"));
        sim_wait(500);
        check("title bar restored to y=0", lv_obj_get_y(title_bar) == 0);
    }

    check("counter shows '1 / 6'", screen_has_label_text(viewer, "1 / 6"));

    /* Horizontal drag must drive the tileview to the next tile.
     * sim_drag drives the real indev, so a regression in the tileview's
     * scroll heuristic or in indev hit-test on a CLICKABLE child rect
     * trips here.  Park the touch one frame before the press so the
     * vect computed from a stale last_point doesn't trip find_scroll_obj
     * onto the wrong widget (same trick as the SMS row tap). */
    int32_t y_mid = CELLPHONE_CONTENT_Y + CELLPHONE_CONTENT_H / 2;
    int32_t x_right = CELLPHONE_HOR_RES - 30;
    int32_t x_left  = 30;
    lv_indev_reset(s_indev, NULL);
    sim_set_touch_point(x_right, y_mid);
    sim_wait(32);
    sim_drag(x_right, y_mid, x_left, y_mid, 250);
    sim_wait(500);
    check("counter advanced to '2 / 6' after horizontal swipe",
          screen_has_label_text(viewer, "2 / 6"));

    cellphone_screen_pop();
    sim_wait(800);
    check("popped viewer", cellphone_screen_depth() == depth_after_thumb);

    cellphone_screen_pop();
    sim_wait(1000);
    check("popped Photos", cellphone_screen_depth() == depth_before);
    printf("  [peak] Photos gestures test peak: %zu KiB\n",
           test_peak_get() / 1024);
}

/** Settings coverage: drilldown into the Display sub-page (catches
 *  lv_menu page navigation regressions) and roller drag on the Time
 *  sub-page (catches roller indev / scroll-snap regressions on touch
 *  MCUs).  test_app("Settings", ...) only smoke-tested push/pop. */
static void test_settings_gestures(void)
{
    printf("\n--- Test: Settings (drilldown + roller) ---\n");
    test_peak_reset();
    int depth_before = cellphone_screen_depth();

    cellphone_screen_push(cellphone_settings_create);
    sim_wait(500);
    check("Settings pushed", cellphone_screen_depth() == depth_before + 1);

    /* Display drilldown: tap the 'Display' menu item.  lv_menu loads the
     * sub-page in place (no cellphone-stack push), so gate on a label
     * exclusive to the sub-page.  'Brightness' lives only inside
     * create_display_page. */
    check("tap 'Display' menu item", sim_tap_label("Display"));
    sim_wait(500);
    check("display sub-page loaded ('Brightness' visible)",
          screen_has_label_text(cellphone_screen_top(), "Brightness"));

    /* Pop and re-push so the menu starts at the main page again -- there
     * is no cellphone-level back from a sub-page, only lv_menu's own
     * header back button which is awkward to target without hard-coded
     * coords. */
    cellphone_screen_pop();
    sim_wait(800);
    cellphone_screen_push(cellphone_settings_create);
    sim_wait(500);

    /* Time drilldown -> roller drag.  The Time sub-page builds two
     * infinite rollers (24/60); find the first roller via class. */
    check("tap 'Time' menu item", sim_tap_label("Time"));
    sim_wait(500);
    check("time sub-page loaded ('Set Time' visible)",
          screen_has_label_text(cellphone_screen_top(), "Set Time"));

    lv_obj_t * roller = find_widget_by_class(cellphone_screen_top(),
                                             &lv_roller_class);
    check("Time page has a roller widget", roller != NULL);
    if(roller) {
        uint32_t before = lv_roller_get_selected(roller);
        printf("  [trace] roller starts at index %" LV_PRIu32 "\n", before);

        lv_area_t a;
        lv_obj_get_coords(roller, &a);
        int32_t cx = (a.x1 + a.x2) / 2;
        int32_t cy = (a.y1 + a.y2) / 2;
        lcd_local_from_global(&cx, &cy);

        /* Drag up by ~one row pitch (CELLPHONE_FONT_LARGE is 22 px so
         * line-height ~26 px).  30 px of vertical drag clears the
         * scroll threshold and snaps to the next row.  Park the touch
         * one frame first so vect on PRESSED doesn't reclassify. */
        lv_indev_reset(s_indev, NULL);
        sim_set_touch_point(cx, cy);
        sim_wait(32);
        sim_drag(cx, cy, cx, cy - 30, 250);
        sim_wait(500);

        uint32_t after = lv_roller_get_selected(roller);
        printf("  [trace] roller now at index %" LV_PRIu32 "\n", after);
        check("roller advanced after vertical drag", after != before);
    }

    cellphone_screen_pop();
    sim_wait(1000);
    check("returned home from Settings",
          cellphone_screen_depth() == depth_before);
    printf("  [peak] Settings gestures test peak: %zu KiB\n",
           test_peak_get() / 1024);
}

/** Theme refresh should repaint the current screen in-place rather than
 *  bouncing back to the lock screen. Gate on three things:
 *  1. stack depth and top screen stay on Contacts,
 *  2. no lock screen reappears,
 *  3. a concrete themed surface (list-button bg) changes to the new CARD. */
static void test_theme_refresh_in_place(void)
{
    printf("\n--- Test: Theme refresh in place ---\n");
    int depth_before;
    lv_obj_t * top;
    lv_obj_t * tileview;
    lv_obj_t * tile;
    lv_obj_t * list;
    lv_obj_t * snake_screen;
    lv_obj_t * snake_status;
    char snake_before[64];
    uint32_t nth;
    lv_coord_t scroll_before;
    int32_t home_page_before;

    cellphone_theme_set(0);
    lv_demo_cellphone_rebuild();
    sim_wait(500);
    cellphone_screen_pop();
    sim_wait(500);
    cellphone_screen_home();
    sim_wait(500);
    depth_before = cellphone_screen_depth();

    top = cellphone_screen_top();
    check("Home visible before theme refresh",
          cellphone_screen_top_is(cellphone_home_create));
    check("Home screen starts with Olive BG",
          top && lv_color_eq(lv_obj_get_style_bg_color(top, 0), cellphone_theme_get(0)->bg));

    cellphone_screen_push(cellphone_settings_create);
    sim_wait(500);
    check("Settings pushed for theme refresh test",
          cellphone_screen_top_is(cellphone_settings_create));

    sim_click(CELLPHONE_HOR_RES / 2, CELLPHONE_CONTENT_Y + 18);
    sim_wait(500);
    check("Display submenu opened before theme refresh",
          find_label_obj(cellphone_screen_top(), "Brightness") != NULL);

    cellphone_theme_set(1);
    lv_demo_cellphone_refresh_theme();
    sim_wait(500);

    top = cellphone_screen_top();
    check("theme refresh keeps Settings on top",
          cellphone_screen_top_is(cellphone_settings_create));
    check("theme refresh does not reopen lock screen",
          !cellphone_screen_top_is(cellphone_lock_create));
    check("theme refresh preserves stack depth", cellphone_screen_depth() == depth_before + 1);
    check("theme refresh preserves Settings submenu",
          find_label_obj(top, "Brightness") != NULL);
    cellphone_screen_pop();
    sim_wait(500);

    top = cellphone_screen_top();
    check("Home screen repaints to Dark BG",
          top && lv_color_eq(lv_obj_get_style_bg_color(top, 0), cellphone_theme_get(1)->bg));

    tileview = find_widget_by_class(top, &lv_tileview_class);
    tile = tileview ? lv_tileview_get_tile_active(tileview) : NULL;
    check("Home tileview present after refresh", tileview != NULL);
    if(tileview) {
        lv_tileview_set_tile_by_index(tileview, 1, 0, LV_ANIM_OFF);
        sim_wait(100);
        tile = lv_tileview_get_tile_active(tileview);
        home_page_before = tile ? (int32_t)(lv_obj_get_x(tile) / lv_obj_get_width(tileview)) : -1;
        cellphone_theme_set(0);
        lv_demo_cellphone_refresh_theme();
        sim_wait(100);
        top = cellphone_screen_top();
        tileview = find_widget_by_class(top, &lv_tileview_class);
        tile = tileview ? lv_tileview_get_tile_active(tileview) : NULL;
        check("theme refresh preserves Home top screen",
              cellphone_screen_top_is(cellphone_home_create));
        check("theme refresh preserves Home page",
              tileview && tile
              && (int32_t)(lv_obj_get_x(tile) / lv_obj_get_width(tileview)) == home_page_before);
    }

    cellphone_screen_push(cellphone_calllog_create);
    sim_wait(500);
    list = find_first_scrollable_descendant(cellphone_screen_top());
    check("Call Log scrollable list present", list != NULL);
    if(list) {
        lv_obj_scroll_to_y(list, CELLPHONE_LIST_ROW_H * 3, LV_ANIM_OFF);
        sim_wait(100);
        scroll_before = lv_obj_get_scroll_y(list);
        cellphone_theme_set(1);
        lv_demo_cellphone_refresh_theme();
        sim_wait(100);
        list = find_first_scrollable_descendant(cellphone_screen_top());
        check("theme refresh keeps Call Log on top",
              cellphone_screen_top_is(cellphone_calllog_create));
        check("theme refresh preserves Call Log scroll position",
              list && lv_obj_get_scroll_y(list) == scroll_before);
    }
    cellphone_screen_pop();
    sim_wait(500);

    cellphone_screen_push(cellphone_snake_create);
    sim_wait(500);
    snake_screen = cellphone_screen_top();
    snake_status = find_label_prefix_obj(snake_screen, "Auto-play");
    check("Snake status label present for theme refresh", snake_status != NULL);
    lv_snprintf(snake_before, sizeof(snake_before), "%s",
                snake_status ? lv_label_get_text(snake_status) : "");
    cellphone_theme_set(0);
    lv_demo_cellphone_refresh_theme();
    sim_wait(20);
    snake_screen = cellphone_screen_top();
    snake_status = find_label_prefix_obj(snake_screen, "Auto-play");
    check("theme refresh keeps Snake on top",
          cellphone_screen_top_is(cellphone_snake_create));
    check("theme refresh preserves Snake state text",
          snake_status && strcmp(lv_label_get_text(snake_status), snake_before) == 0);
    cellphone_screen_pop();
    sim_wait(500);
}

static void test_contacts_add_focus_chain(void)
{
    printf("\n--- Test: Contacts add focus chain ---\n");
    lv_obj_t * name_ta;
    lv_obj_t * phone_ta;
    lv_obj_t * email_ta;

    cellphone_screen_push(cellphone_contacts_create);
    sim_wait(500);
    check("Contacts pushed for add-focus test",
          cellphone_screen_top_is(cellphone_contacts_create));
    check("add-contact overlay opens", cellphone_contacts_test_open_add_overlay());
    name_ta = cellphone_contacts_test_get_add_field(0);
    phone_ta = cellphone_contacts_test_get_add_field(1);
    email_ta = cellphone_contacts_test_get_add_field(2);
    check("add-contact form exposes three textareas",
          name_ta && phone_ta && email_ta);
    if(name_ta && phone_ta && email_ta) {
        lv_obj_add_state(name_ta, LV_STATE_FOCUSED);
        check("focus advances from Name to Phone",
              cellphone_contacts_test_advance_add_focus(0));
        check("Name field clears focus when moving to Phone",
              !lv_obj_has_state(name_ta, LV_STATE_FOCUSED));
        check("Phone field gains focus after Name READY",
              lv_obj_has_state(phone_ta, LV_STATE_FOCUSED));

        check("focus advances from Phone to Email",
              cellphone_contacts_test_advance_add_focus(1));
        check("Phone field clears focus when moving to Email",
              !lv_obj_has_state(phone_ta, LV_STATE_FOCUSED));
        check("Email field is sole focused field after second READY",
              lv_obj_has_state(email_ta, LV_STATE_FOCUSED)
              && !lv_obj_has_state(name_ta, LV_STATE_FOCUSED));
    }

    cellphone_screen_pop();
    sim_wait(500);
}

static void test_contacts_mutation_lifecycle(void)
{
    printf("\n--- Test: Contacts mutation lifecycle ---\n");

    lv_demo_cellphone_rebuild();
    sim_wait(500);
    cellphone_screen_pop();
    sim_wait(500);

    check("contacts reset to stock count on rebuild",
          cellphone_data_contact_count() == CELLPHONE_CONTACT_COUNT);

    int32_t idx = cellphone_data_contact_add("aaron", "+1-555-0199", "aaron@example.com");
    const cellphone_contact_t * c0 = cellphone_data_contact_at(0);
    check("lowercase contact inserts successfully", idx >= 0);
    check("lowercase contact sorts with uppercase A section",
          idx == 0 && c0 && lv_strcmp(c0->name, "aaron") == 0);

    check("contact count increments after add",
          cellphone_data_contact_count() == CELLPHONE_CONTACT_COUNT + 1);

    lv_demo_cellphone_rebuild();
    sim_wait(500);
    cellphone_screen_pop();
    sim_wait(500);

    c0 = cellphone_data_contact_at(0);
    check("fresh rebuild restores stock contact count",
          cellphone_data_contact_count() == CELLPHONE_CONTACT_COUNT);
    check("fresh rebuild removes added contact",
          c0 && lv_strcmp(c0->name, "Ashley Johnson") == 0);
}

static void test_arcade_timer_visibility(void)
{
    printf("\n--- Test: Arcade timer visibility ---\n");
    lv_obj_t * snake_screen;
    lv_obj_t * snake_status;
    char status_before[64];
    const char * status_hidden;

    cellphone_screen_home();
    sim_wait(500);

    cellphone_screen_push(cellphone_snake_create);
    sim_wait(500);
    snake_screen = cellphone_screen_top();
    check("Snake pushed", cellphone_screen_top_is(cellphone_snake_create));

    snake_status = find_label_prefix_obj(snake_screen, "Auto-play");
    check("Snake status label present", snake_status != NULL);
    lv_snprintf(status_before, sizeof(status_before), "%s",
                snake_status ? lv_label_get_text(snake_status) : "");

    cellphone_screen_push(cellphone_calc_create);
    sim_wait(500);
    check("Calculator covers Snake", cellphone_screen_top_is(cellphone_calc_create));

    status_hidden = snake_status ? lv_label_get_text(snake_status) : NULL;
    check("hidden Snake stops updating state text",
          status_hidden && strcmp(status_before, status_hidden) == 0);

    cellphone_screen_pop();
    sim_wait(500);
    check("returned to Snake", cellphone_screen_top_is(cellphone_snake_create));
    cellphone_screen_pop();
    sim_wait(500);
}

/** Walk the active contacts and call-log lists and assert every visible
 *  row paints with the active theme's CARD color.  Catches regressions
 *  that drop the explicit `cellphone_obj_paint_fill(btn, CARD)` override
 *  in contacts/calllog row builders, which would otherwise be visible
 *  only by eye on a manual demo run.  Run once per theme so palette-
 *  specific drift (Olive cream vs Dark gray) is asserted in both
 *  configurations. */
static void test_list_button_colors_for_theme(uint32_t theme_idx,
                                              const char * theme_name)
{
    printf("\n--- Test: list-button color (%s theme) ---\n", theme_name);
    int depth_before = cellphone_screen_depth();

    cellphone_theme_set(theme_idx);
    /* lv_demo_cellphone_rebuild() rebuilds the tree against the new
     * palette so the LVGL theme primary and the demo's per-row CARD
     * override agree.  Without rebuild, the active screen still carries
     * the previous theme's primary -- harmless for our explicit-bg
     * gate, but stale-theme widgets would skew any styling regression
     * that surfaces only through the LVGL theme cascade. */
    lv_demo_cellphone_rebuild();
    sim_wait(500);
    /* Rebuild restarts at the lock screen -- slide-to-unlock to reach
     * the home + chrome state where push() is meaningful. */
    cellphone_screen_pop();
    sim_wait(500);

    const cellphone_theme_t * theme = cellphone_theme_active();
    lv_color_t card = theme->card;

    /* Contacts: lv_list_button rows. */
    cellphone_screen_push(cellphone_contacts_create);
    sim_wait(500);
    uint32_t total = 0, mismatched = 0;
    list_buttons_check_card(cellphone_screen_top(), card, &total, &mismatched);
    {
        char msg[96];
        lv_snprintf(msg, sizeof(msg),
                    "contacts list-buttons paint with %s CARD (%" LV_PRIu32
                    "/%" LV_PRIu32 " match)",
                    theme_name, total - mismatched, total);
        check(msg, total > 0 && mismatched == 0);
    }
    cellphone_screen_pop();
    sim_wait(500);

    /* Calllog: clickable flex-row containers under a vert-scroll list. */
    cellphone_screen_push(cellphone_calllog_create);
    sim_wait(500);
    total = 0;
    mismatched = 0;
    calllog_rows_check_card(cellphone_screen_top(), card, &total, &mismatched);
    {
        char msg[96];
        lv_snprintf(msg, sizeof(msg),
                    "calllog rows paint with %s CARD (%" LV_PRIu32
                    "/%" LV_PRIu32 " match)",
                    theme_name, total - mismatched, total);
        check(msg, total > 0 && mismatched == 0);
    }
    cellphone_screen_pop();
    sim_wait(500);

    check("depth restored after list-button color check",
          cellphone_screen_depth() == depth_before);
}

/** Re-run a scaled-down push/pop matrix under the Dark theme so palette-
 *  specific regressions register with the same structural / pool gates
 *  as the Olive cycle batch.  Switches the theme via cellphone_theme_set
 *  + lv_demo_cellphone_rebuild, runs two cycles (warmup + steady), then
 *  restores Olive.  Two cycles is enough to catch a per-cycle leak
 *  without paying the 5-cycle cost twice. */
static void test_dark_theme_cycle(void)
{
    printf("\n--- Test 11b: Dark theme cycle pass ---\n");
    test_peak_reset();

    cellphone_screen_create_fn cycle_fns[] = {
        cellphone_dialer_create,   cellphone_contacts_create,
        cellphone_sms_create,      cellphone_calc_create,
        cellphone_calllog_create,
    };
    const int cycle_count = (int)(sizeof(cycle_fns) / sizeof(cycle_fns[0]));
    const int n_cycles = 2;

    cellphone_theme_set(1);  /* Dark */
    lv_demo_cellphone_rebuild();
    sim_wait(500);
    cellphone_screen_pop();  /* dismiss lock screen */
    sim_wait(500);

    int depth_baseline = cellphone_screen_depth();
    size_t end_used[2] = {0, 0};

    for(int n = 0; n < n_cycles; n++) {
        for(int i = 0; i < cycle_count; i++) {
            lv_display_trigger_activity(s_disp);
            cellphone_screen_push(cycle_fns[i]);
            sim_wait(400);
            cellphone_screen_pop();
            sim_wait(700);
        }
        end_used[n] = mem_used_now();
        printf("  Dark cycle %d: end-of-cycle used=%zu KiB\n",
               n, end_used[n] / 1024);
    }

    check("Dark cycle: depth restored after batch",
          cellphone_screen_depth() == depth_baseline);

    /* Tighter budget than Olive: Dark only runs 2 cycles, so the gate
     * is across cycle 0 and cycle 1 deltas.  Cycle 0 is warmup (theme
     * styles, vec font cache fill); cycle 1 should be steady-state.
     * 16 KiB tolerance leaves room for first-touch glyph rasterization
     * on Dark-specific surfaces (dropdowns, slider thumbs) without
     * masking a real per-cycle leak. */
    size_t d01 = end_used[1] >= end_used[0]
                 ? end_used[1] - end_used[0] : 0;
    const size_t budget = 16 * 1024;
    if(d01 > budget) {
        char msg[128];
        lv_snprintf(msg, sizeof(msg),
                    "Dark cycle: 0->1 growth too high: %zu KiB, budget %zu KiB",
                    d01 / 1024, budget / 1024);
        check(msg, false);
    }
    else {
        check("Dark cycle: post-warmup pool stable", true);
    }

    printf("  [peak] Dark cycle test peak: %zu KiB\n",
           test_peak_get() / 1024);

    /* Restore Olive so the rest of the run continues against the
     * baseline palette.  Rebuild leaves us on the lock screen; pop it
     * so subsequent assertions see a sensible depth. */
    cellphone_theme_set(0);
    lv_demo_cellphone_rebuild();
    sim_wait(500);
    cellphone_screen_pop();
    sim_wait(500);
}

/**********************
 *       MAIN
 **********************/

int main(int argc, char ** argv)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    for(int i = 1; i < argc; i++) {
        if(strcmp(argv[i], "--focused") == 0) {
            s_focused_mode = true;
        }
    }

    printf("=== Cell Phone Demo - UI Validation ===\n");

    lv_init();

    int32_t win_w = SCR_W;
    int32_t win_h = SCR_H;
#if defined(LV_DEMO_CELLPHONE_SKIN) && LV_DEMO_CELLPHONE_SKIN && LV_USE_SDL
    cellphone_skin_get_window_size(&win_w, &win_h);
#endif
    s_disp = lv_sdl_window_create(win_w, win_h);
    lv_sdl_window_set_title(s_disp, "Cell Phone Demo - Validation");

    /* lv_sdl_window_create installs SDL_GetTicks as the tick source and
     * SDL_Delay as the delay source. That makes lv_tick_inc() a no-op, so
     * sim_wait() advances neither the animation timer nor
     * lv_obj_delete_delayed(); future lv_delay_ms() calls would also block
     * on wall time. Clear both so simulated time drives the demo
     * deterministically. */
    lv_tick_set_cb(NULL);
    lv_delay_set_cb(NULL);

    /* Create a simulated touch input device */
    s_indev = lv_indev_create();
    lv_indev_set_type(s_indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(s_indev, touch_read_cb);
    lv_indev_set_display(s_indev, s_disp);
    cellphone_screen_set_transitions_enabled(false);

    sim_wait(100);

    /* Test 01: Boot */
    test_01_boot();

    /* Test 02: Unlock */
    test_02_unlock();

    /* Test 03-10: Push each app, gate, pop back */
    /* Navbar click path: push a screen, then dismiss it via the navbar's
     * middle tab (which is contextually "Back" when depth > 1).  Catches
     * regressions in chrome-button hit-test, click event delivery, and any
     * future widget-class swap. */
    {
        printf("\n--- Test: Navbar Back click ---\n");
        int depth_before = cellphone_screen_depth();
        cellphone_screen_push(cellphone_calc_create);
        sim_wait(500);
        check("pre-tap depth +1", cellphone_screen_depth() == depth_before + 1);

        /* Keep one real pointer click smoke test for navbar hit delivery. */
        sim_click(CELLPHONE_HOR_RES / 2, CELLPHONE_NAVBAR_Y + CELLPHONE_NAVBAR_H / 2);
        sim_wait(500);
        check("navbar Back popped one level",
              cellphone_screen_depth() == depth_before);
    }

    /* Root tabs are destinations, not history entries. Switching between
     * them from deeper screens must collapse stale tab roots so Back still
     * means "return one level" instead of "walk old tabs in reverse". */
    {
        printf("\n--- Test: Navbar tab switch resets branch ---\n");
        navbar_tab_click_wait_or_fail(2, "clicked Contacts tab",
                                      cellphone_contacts_create, 2,
                                      "Contacts tab opened at depth 2");

        navbar_tab_click_wait_or_fail(0, "clicked Dialer tab",
                                      cellphone_dialer_create, 2,
                                      "Dialer tab replaced Contacts branch");

        navbar_tab_click_wait_or_fail(2, "clicked Contacts tab again",
                                      cellphone_contacts_create, 2,
                                      "Contacts tab replaced Dialer branch");

        navbar_tab_click_wait_or_fail(1, "clicked middle tab",
                                      NULL, 1,
                                      "navbar Back from root tab returns to Home");
    }

    test_dialer_keypad();
    test_calc_keypad();
    test_app("Phone",      cellphone_dialer_create);
    test_app("Contacts",   cellphone_contacts_create);
    if(!s_focused_mode) {
        /* SMS chat-detail view: the thread list is one screen, a chat detail
         * (bubbles) is reached by tapping a thread.  test_app only covers the
         * thread list, so cover the bubble path here. */
        printf("\n--- Test: SMS chat detail (bubble layout) ---\n");
        int depth_before = cellphone_screen_depth();
        cellphone_screen_push(cellphone_sms_create);
        sim_wait(500);
        check("SMS thread list pushed", cellphone_screen_depth() == depth_before + 1);

        /* Park the touch at the row position with state=RELEASED for
         * one frame before pressing.  The dialer/calc tests dispatch
         * CLICKED via lv_obj_send_event (bypassing the indev), which
         * means LVGL's `last_point` still holds the stale coordinate
         * from the navbar test ((120, ~299)).  Without this settle,
         * the upcoming sim_press computes vect = row - last_point >
         * scroll_limit, find_scroll_obj climbs to the SMS list (which
         * is vertically scrollable), drops the row's PRESSED, and
         * indev_proc_release suppresses CLICKED.  An lv_indev_reset
         * alone isn't enough -- it clears act_obj / scroll_obj but
         * leaves last_point untouched. */
        lv_indev_reset(s_indev, NULL);
        sim_set_touch_point(CELLPHONE_HOR_RES / 2, CELLPHONE_CONTENT_Y + 40);
        sim_wait(32);
        sim_click(CELLPHONE_HOR_RES / 2, CELLPHONE_CONTENT_Y + 40);
        sim_wait(500);
        check("chat detail pushed via row tap",
              cellphone_screen_depth() == depth_before + 2);

        lv_obj_t * detail = cellphone_screen_top();
        if(detail) {
            uint32_t tree = obj_tree_count(detail);
            printf("  (chat detail tree: %" LV_PRIu32 " nodes)\n", tree);
            check("chat detail has bubbles", tree > 5);
        }

        cellphone_screen_pop();
        sim_wait(500);
        cellphone_screen_pop();
        sim_wait(500);
        check("returned to home after chat", cellphone_screen_depth() == depth_before);
    }
    if(!s_focused_mode) test_app("Messages", cellphone_sms_create);
    test_app("Calculator", cellphone_calc_create);
    if(!s_focused_mode) test_app("Music",      cellphone_music_create);
    if(!s_focused_mode) test_photos_gestures();
    if(!s_focused_mode) test_settings_gestures();
    if(!s_focused_mode) test_theme_refresh_in_place();
    if(!s_focused_mode) test_contacts_add_focus_chain();
    if(!s_focused_mode) test_contacts_mutation_lifecycle();
    if(!s_focused_mode) test_arcade_timer_visibility();
    test_app("Call Log",    cellphone_calllog_create);

    if(s_focused_mode) {
        printf("\n=== Summary ===\n");
        printf("PASS: %d\nFAIL: %d\n", s_pass, s_fail);
        return s_fail == 0 ? 0 : 1;
    }

    /* Test 11: Pool growth across cycles. Drives push/pop on every app
     * five times and gates leak risk on the last two cycle-to-cycle deltas
     * plus a structural invariant.
     *
     * Cycle 0 has legitimate one-time costs (theme styles, statics, vec font
     * bitmap cache fill on first render). The vec font cache also keeps
     * filling for a few cycles when LRU eviction churns: screens hit unique
     * glyph sets only on later visits (e.g. Settings rollers populate
     * "00".."59" after dropdown text gets evicted). Cycles 0-2 are warmup;
     * cycles 3 and 4 should be stable. Gating on the last two deltas with
     * an 8 KiB tolerance comfortably distinguishes residual cache noise
     * from a real per-cycle leak (an actually-broken cellphone_screen_pop
     * grows the pool by ~95 KiB/cycle).
     *
     * The screen-depth invariant is independent of the pool measurement:
     * any push/pop imbalance is caught even if memory accidentally lines
     * up.
     *
     * In addition to the end-of-cycle deltas, the allocator-tracked
     * `lv_mem_monitor_t.max_used` gives the true high-water mark across
     * the whole test, including transients within a single frame that
     * frame-end polling would miss. The peak is the number that
     * `LV_MEM_SIZE` on a target build has to cover, not the steady
     * state. */
    {
        printf("\n--- Test 11: Pool growth across cycles ---\n");
        cellphone_screen_create_fn cycle_fns[] = {
            cellphone_dialer_create,   cellphone_contacts_create,
            cellphone_sms_create,      cellphone_calc_create,
            cellphone_music_create,    cellphone_photo_create,
            cellphone_settings_create, cellphone_calllog_create,
        };
        const int cycle_count = (int)(sizeof(cycle_fns) / sizeof(cycle_fns[0]));
        const int n_cycles = 5;

        int depth_baseline = cellphone_screen_depth();
        size_t end_used[5];

        /* Snapshot the L2 cache counters so the vector-font-cache phase
         * gates on the Test 11 delta, not the process-lifetime sum. The
         * earlier per-app phase (Tests 01-10) pads the cumulative ratio
         * with one-shot renders that don't reflect the cycle-batch
         * workload we actually want to gate. Guarded on LV_USE_FONT_VEC
         * because the accessor only exists in that build; a non-vec
         * build skips the gate entirely (the matching read-back in the
         * vector-font-cache phase is also LV_USE_FONT_VEC-gated). */
        s_l2_hits_pre_test11 = 0;
        s_l2_misses_pre_test11 = 0;
#if LV_USE_FONT_VEC
        lv_font_vec_get_l2_stats(&s_l2_hits_pre_test11, &s_l2_misses_pre_test11);
#endif

        for(int n = 0; n < n_cycles; n++) {
            for(int i = 0; i < cycle_count; i++) {
                /* Total batch runtime exceeds the 30 s idle-lock timeout, so
                 * reset the inactivity counter each iteration to keep the
                 * lock screen from auto-pushing mid-test. */
                lv_display_trigger_activity(s_disp);
                cellphone_screen_push(cycle_fns[i]);
                sim_wait(500);

                /* SMS gets an extra drill into chat detail per cycle.  The
                 * chat-detail render isn't reachable from cycle_fns[] (it's
                 * pushed by a row tap), and a single one-shot probe outside
                 * the cycle batch only feeds the peak watermark -- a per-
                 * open chat-detail leak (e.g. forgotten event handler, label
                 * buffer left allocated) wouldn't show up in the d23/d34
                 * delta gate because it'd be amortized into cycle 0's
                 * warmup.  Folding it inside the loop puts every chat-detail
                 * push/pop on the same delta-budget footing as the root
                 * apps. */
                if(cycle_fns[i] == cellphone_sms_create) {
                    /* See SMS chat-detail probe earlier: reset + settle so
                     * the row tap isn't classified as a scroll on the
                     * SMS list. */
                    lv_indev_reset(s_indev, NULL);
                    sim_set_touch_point(CELLPHONE_HOR_RES / 2, CELLPHONE_CONTENT_Y + 40);
                    sim_wait(32);
                    sim_click(CELLPHONE_HOR_RES / 2, CELLPHONE_CONTENT_Y + 40);
                    sim_wait(500);
                    cellphone_screen_pop();  /* chat detail */
                    sim_wait(500);
                }

                cellphone_screen_pop();
                sim_wait(1000);
            }
            lv_mem_monitor_t m;
            lv_mem_monitor(&m);
            end_used[n] = m.free_size <= m.total_size
                          ? (size_t)(m.total_size - m.free_size)
                          : 0;
            printf("  cycle %d: end-of-cycle used=%zu KiB\n",
                   n, end_used[n] / 1024);
            if(n > 0) {
                bool grew = end_used[n] >= end_used[n - 1];
                size_t delta_kb = (grew ? end_used[n] - end_used[n - 1]
                                   : end_used[n - 1] - end_used[n]) / 1024;
                printf("    delta cycle %d->%d: %c%zu KiB\n",
                       n - 1, n, grew ? '+' : '-', delta_kb);
            }
        }

        check("screen depth restored after cycle batch",
              cellphone_screen_depth() == depth_baseline);

        const size_t budget = 8 * 1024;
        size_t d34 = end_used[4] >= end_used[3]
                     ? end_used[4] - end_used[3] : 0;
        size_t d23 = end_used[3] >= end_used[2]
                     ? end_used[3] - end_used[2] : 0;
        if(d34 > budget || d23 > budget) {
            char msg[128];
            lv_snprintf(msg, sizeof(msg),
                        "post-warmup growth too high: 2->3 %zu KiB, "
                        "3->4 %zu KiB, budget %zu KiB",
                        d23 / 1024, d34 / 1024, budget / 1024);
            check(msg, false);
        }
        else {
            check("post-warmup pool stable across cycles 3 and 4", true);
        }

        /* Peak gate uses the allocator's max_used watermark, which is
         * updated on every alloc/realloc and so captures intra-frame
         * transients (e.g. ThorVG rasterizer scratch buffers, draw-task
         * descriptors) that sampling on frame boundaries would miss.
         * The watermark is never reset, so this is the global peak
         * across the whole run -- which is exactly the worst case an
         * MCU integrator has to size LV_MEM_SIZE for. */
        size_t pool_peak = mem_peak_used();
        printf("  peak pool occupancy across full run: %zu KiB\n",
               pool_peak / 1024);
        /* Ceiling tracks lv_conf_mcu.h LV_MEM_SIZE.  Update both in
         * lockstep -- the gate must equal the shipping pool size minus
         * 0 KiB (the gate enforces the budget directly, not headroom). */
        const size_t peak_ceiling = 76 * 1024;
        if(pool_peak > peak_ceiling) {
            char msg[128];
            lv_snprintf(msg, sizeof(msg),
                        "pool peak %zu KiB exceeds ceiling %zu KiB; "
                        "review lv_conf_mcu.h LV_MEM_SIZE",
                        pool_peak / 1024, peak_ceiling / 1024);
            check(msg, false);
        }
        else {
            check("pool peak below MCU profile ceiling (76 KiB)", true);
        }
    }

#if LV_USE_FONT_VEC
    /* Test: Vector font cache validation */
    {
        printf("\n--- Test: Vector Font Cache ---\n");

        const cellphone_theme_t * theme = cellphone_theme_active();
        check("vec font_sm set",      theme->font_sm != NULL);
        check("vec font_normal set",  theme->font_normal != NULL);
        check("vec font_heading set", theme->font_heading != NULL);
        check("vec font_large set",   theme->font_large != NULL);
        check("vec font_clock set",   theme->font_clock != NULL);

        lv_font_vec_log_stats();

        /* Verify each vec font has valid line_height */
        check("font_sm line_height > 0",      theme->font_sm->line_height > 0);
        check("font_normal line_height > 0",  theme->font_normal->line_height > 0);
        check("font_clock line_height > 0",   theme->font_clock->line_height > 0);

        /* L2 cache hit-rate gate, computed as the Test-11 delta. The
         * configured caps (sm/normal at 2 KiB) keep this above ~70% on
         * the cellphone workload; a drop below 60% means the cache is
         * thrashing and on a slow MCU the doubled rasterization rate
         * would burn frame budget. Gating on the delta (not the
         * process-lifetime cumulative) keeps the per-app phase warmup
         * and chat-detail probe from padding the ratio and masking
         * cycle-batch churn -- those are one-shot renders, not the
         * sustained workload we actually care about.
         *
         * The ceiling catches the inverse failure mode: a sudden jump
         * to near-100% almost always means the test workload shrank
         * (an app dropped, a roller stopped enumerating its full set,
         * etc.) and the cache is "winning" only because the working set
         * collapsed.  Without a ceiling that regression slips past as a
         * "cache improvement" until somebody spots the missing coverage
         * by hand.  95% is the loosest bound that still flags such a
         * collapse on the current 240x320 cellphone workload (typical
         * delta is ~71%, so ~24 percentage points of headroom). */
        uint32_t l2_hits = 0, l2_misses = 0;
        lv_font_vec_get_l2_stats(&l2_hits, &l2_misses);
        uint32_t l2_hits_delta   = l2_hits   - s_l2_hits_pre_test11;
        uint32_t l2_misses_delta = l2_misses - s_l2_misses_pre_test11;
        uint32_t l2_total_delta  = l2_hits_delta + l2_misses_delta;
        check("L2 hit-rate delta sample is non-empty", l2_total_delta > 0);
        if(l2_total_delta > 0) {
            uint32_t l2_pct = (uint32_t)((uint64_t)l2_hits_delta * 1000u
                                         / (uint64_t)l2_total_delta);
            printf("  L2 hit rate (Test 11 delta): %" LV_PRIu32 ".%" LV_PRIu32 "%%"
                   " over %" LV_PRIu32 " fetches\n",
                   l2_pct / 10u, l2_pct % 10u, l2_total_delta);
            char msg[96];
            lv_snprintf(msg, sizeof(msg),
                        "L2 hit rate >= 60.0%% (got %" LV_PRIu32 ".%" LV_PRIu32 "%%)",
                        l2_pct / 10u, l2_pct % 10u);
            check(msg, l2_pct >= 600);
            lv_snprintf(msg, sizeof(msg),
                        "L2 hit rate <= 95.0%% (got %" LV_PRIu32 ".%" LV_PRIu32 "%%)",
                        l2_pct / 10u, l2_pct % 10u);
            check(msg, l2_pct <= 950);
        }

        /* Verify cache budget compliance on each vec font instance.
         * The expected max sizes mirror the per-font budgets configured
         * in vec_fonts_init() in lv_demo_cellphone.c. A regression that
         * drifts a budget without changing peak enough to trip the
         * cycle gate would slip past `used <= max` alone, so assert
         * the configured caps directly. 0 means "cache disabled". */
        const lv_font_t * vec_fonts[] = {
            theme->font_sm, theme->font_normal, theme->font_heading,
            theme->font_large, theme->font_clock
        };
        const char * vec_names[] = {"sm(12)", "normal(14)", "heading(16)", "large(22)", "clock(32)"};
        const size_t vec_expected_max[] = {
            LV_DEMO_CELLPHONE_FONT_CACHE_SM,
            LV_DEMO_CELLPHONE_FONT_CACHE_NORMAL,
            LV_DEMO_CELLPHONE_FONT_CACHE_HEADING,
            LV_DEMO_CELLPHONE_FONT_CACHE_LARGE,
            LV_DEMO_CELLPHONE_FONT_CACHE_CLOCK
        };

        for(int i = 0; i < 5; i++) {
            int32_t px = lv_font_vec_get_instance_pixel_size(vec_fonts[i]);
            if(px > 0) {
#if LV_FONT_VEC_CACHE_SIZE > 0
                char msg[64];
                size_t used = 0, max = 0;
                bool has_l2 = lv_font_vec_get_instance_l2_size(vec_fonts[i], &used, &max);
                if(vec_expected_max[i] == 0) {
                    lv_snprintf(msg, sizeof(msg), "cache %s disabled (cache_size=0)", vec_names[i]);
                    check(msg, !has_l2);
                }
                else if(has_l2) {
                    printf("  cache %s: %" LV_PRIu32 "/%" LV_PRIu32 " bytes\n",
                           vec_names[i], (uint32_t)used, (uint32_t)max);
                    lv_snprintf(msg, sizeof(msg), "cache %s within budget", vec_names[i]);
                    check(msg, used <= max);
                    lv_snprintf(msg, sizeof(msg), "cache %s configured at %zu B",
                                vec_names[i], vec_expected_max[i]);
                    check(msg, max == vec_expected_max[i]);
                }
                else {
                    /* Expected enabled but the instance reports no live L2
                     * -- the budget split in vec_fonts_init() drifted to
                     * cache_size=0 for this font, or cache create failed at
                     * init. Without this branch the for-loop would silently
                     * skip both gates and the regression would slip past CI. */
                    lv_snprintf(msg, sizeof(msg),
                                "cache %s expected enabled (>=%zu B) but is NULL",
                                vec_names[i], vec_expected_max[i]);
                    check(msg, false);
                }
#endif
                printf("  pixel_size %s: %" LV_PRId32 "\n", vec_names[i], px);
            }
        }
    }
#endif /* LV_USE_FONT_VEC */

    /* List-button color gates run AFTER the L2 hit-rate gate so
     * cellphone_theme_set + lv_demo_cellphone_rebuild churn doesn't pad
     * the Test-11-delta sample with theme-rebuild glyph misses. The
     * gate walks contacts and call-log under each theme and asserts
     * every visible row resolves bg_color to the active theme's CARD --
     * catches regressions in the row helper that would otherwise only
     * surface as a visual review miss. */
    if(!s_focused_mode) {
        test_list_button_colors_for_theme(0, "Olive");
        test_list_button_colors_for_theme(1, "Dark");
    }

    /* Dark theme cycle pass: re-run a scaled-down push/pop matrix under
     * the Dark palette so theme-specific regressions register on the
     * structural / pool gates. Restores Olive at the end. Placed after
     * the list-button color gate so the rebuild churn from theme
     * switching is amortized across both. */
    if(!s_focused_mode) {
        test_dark_theme_cycle();
    }

    /* Optional heap breakdown via mem_report.c (link with -DCELLPHONE_TEST_REPORT). */
#ifdef CELLPHONE_TEST_REPORT
    cellphone_mem_report();
#endif

    /* Summary */
    printf("\n=== Results ===\n");
    printf("  Pass: %d\n", s_pass);
    printf("  Fail: %d\n", s_fail);
    printf("  Total: %d\n", s_pass + s_fail);
    printf("  STATUS: %s\n", s_fail == 0 ? "ALL PASS" : "SOME FAILED");

    lv_sdl_quit();
    return s_fail > 0 ? 1 : 0;
}
