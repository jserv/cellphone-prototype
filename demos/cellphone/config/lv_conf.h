/**
 * @file lv_conf.h
 * Demo-local LVGL configuration for the cellphone SDL simulator.
 *
 * Opt in with:
 *   cmake -DLV_BUILD_CONF_DIR=demos/cellphone/config ...
 * or add this directory to your include path with LV_CONF_INCLUDE_SIMPLE.
 */

#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_COLOR_DEPTH 16
#define LV_MEM_SIZE (1024 * 1024)

#define LV_USE_LOG 1
#if LV_USE_LOG
    #define LV_LOG_LEVEL LV_LOG_LEVEL_ERROR
    #define LV_LOG_PRINTF 1
#endif

/*
 * Vector font engine: Montserrat TTF outlines for all text + FA5 icons.
 * Zero bitmap fonts. LV_FONT_DEFAULT uses a minimal stub (no glyphs)
 * that gets overridden by vec font instances at runtime.
 */
#define LV_USE_FONT_VEC 1
/* L2 bitmap cache: per-instance fill across the demo screens peaks at
 * 4051 B (sm) and 3976 B (normal). 4 KiB budget absorbs both with
 * minor LRU pressure for *this* demo's glyph mix (English + digits +
 * a small icon set). 5 sizes * 4 KiB = 20 KiB total L2.
 *
 * Workloads that render denser text -- long messages, multi-language
 * (CJK), or larger alphabets at sm/normal sizes -- can churn at 4 KiB
 * and re-rasterize evicted glyphs every frame. If `cellphone_test`
 * Test 11 shows L2 fill at the cap with eviction churn, bump this to
 * 8192. The cache is per-font-instance, so a 4 KiB → 8 KiB bump costs
 * 4 KiB × 5 instances = 20 KiB total. Re-measure end-of-cycle and
 * peak afterwards.
 *
 * L1 metrics cache: 2-way set-associative, process-wide shared.
 * One ~4.4 KiB table is keyed on (font_data, pixel_size, unicode), so
 * all live vec font instances share a single allocation -- 32 sets
 * total, irrespective of font count. The previous 64-set sizing cost
 * twice the heap with no measurable win for this glyph mix. */
#define LV_FONT_VEC_CACHE_SIZE 4096
#define LV_FONT_VEC_CACHE_L1_SETS 16

/* Zero bitmap fonts. Vec engine handles all text (Montserrat TTF outlines)
 * and FA5 icons. Stub font for LV_FONT_DEFAULT (defined in lv_font_vec.c). */
#define LV_FONT_MONTSERRAT_14 0
struct _lv_font_t;
extern const struct _lv_font_t lv_font_vec_stub;
#define LV_FONT_DEFAULT ((const lv_font_t *)&lv_font_vec_stub)

#define LV_USE_SDL 1
#define LV_SDL_INCLUDE_PATH <SDL.h>
#define LV_SDL_RENDER_MODE LV_DISPLAY_RENDER_MODE_FULL
#define LV_SDL_BUF_COUNT 1

#define LV_USE_FLEX 1
#define LV_USE_GRID 1
#define LV_USE_SNAPSHOT 1
#define LV_GRADIENT_MAX_STOPS 6

#define LV_BUILD_DEMOS 1
#define LV_USE_DEMO_CELLPHONE 1
#define LV_DEMO_CELLPHONE_SKIN 1

#endif /* LV_CONF_H */
