/**
 * @file lv_font_vec.h
 *
 * Vector font engine derived from mado's fixed-point rasterizer.
 * Renders compact glyph outlines to A8 bitmaps at any pixel size
 * without FPU, FreeType, or ThorVG.
 *
 * Gated by LV_USE_FONT_VEC.
 */

#ifndef LV_FONT_VEC_H
#define LV_FONT_VEC_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/

#include "lv_font.h"
#include "../lv_types.h"

#if LV_USE_FONT_VEC

/*********************
 *      DEFINES
 *********************/

/** Characters per charmap page (must match converter output). */
#define LV_FONT_VEC_PAGE_SHIFT   7
#define LV_FONT_VEC_PER_PAGE     (1 << LV_FONT_VEC_PAGE_SHIFT)

/** Font type: stroke (pen-convolved centerlines) or TTF (filled outlines). */
#define LV_FONT_VEC_TYPE_STROKE  1
#define LV_FONT_VEC_TYPE_TTF     2

/**********************
 *      TYPEDEFS
 **********************/

/**
 * Character map page. Each page covers LV_FONT_VEC_PER_PAGE (128)
 * consecutive Unicode code points.  offsets[i] indexes into the
 * shared outlines array; 0 means "glyph not present".
 */
typedef struct {
    uint32_t page;
    uint32_t offsets[LV_FONT_VEC_PER_PAGE];
} lv_font_vec_charmap_t;

/**
 * Shared font data -- const/ROM-safe, one instance per typeface.
 * Multiple per-instance descriptors (one per pixel size) point here.
 *
 * Glyph coordinates in outlines are Q1.6 signed fixed-point
 * (range -2 .. +1.984375).  ascender/descender/height use the
 * same format.
 *
 * Stroke fonts: glyph header is 6 bytes (left, right, ascent, descent,
 * n_snap_x, n_snap_y) + snap points, then drawing commands.
 * TTF fonts: glyph header is 1 byte (advance), then drawing commands.
 */
typedef struct {
    const lv_font_vec_charmap_t * charmap;
    int32_t                       n_charmap;
    const int8_t                * outlines;      /**< Signed char command stream */
    uint32_t                      outlines_size; /**< Byte length of outlines[] for bounds checking */
    uint8_t                       type;          /**< LV_FONT_VEC_TYPE_STROKE or _TTF */
    int8_t                        ascender;      /**< Q1.6 (TTF only; stroke reads from glyph header) */
    int8_t                        descender;     /**< Q1.6 */
    int8_t                        height;        /**< Q1.6 */
} lv_font_vec_data_t;

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**
 * Initialize a vector font instance at a given pixel size.
 *
 * All lv_font_t callbacks and metrics are set.  The caller must
 * keep @p data alive for the lifetime of the font.
 *
 * Uses the compile-time `LV_FONT_VEC_CACHE_SIZE` for the L2 bitmap
 * cache budget. For per-instance budgets, use `lv_font_vec_init_ex()`.
 *
 * @param font      Font object to populate (caller-owned storage).
 * @param data      Shared outline data (typically const/ROM).
 * @param px        Desired pixel size (line height).
 * @return LV_RESULT_OK on success.
 */
lv_result_t lv_font_vec_init(lv_font_t * font, const lv_font_vec_data_t * data, int32_t px);

/**
 * Initialize a vector font instance with an explicit L2 bitmap cache budget.
 *
 * Same as `lv_font_vec_init()` but lets the caller size the per-instance
 * cache. Useful when font instances differ in working-set size: a small
 * UI (status-bar clock, settings rollers) can run on 2 KiB while denser
 * body text wants 4 KiB. The `cache_size = 0` case disables the cache
 * for this instance only -- glyph fetches fall through to on-demand
 * rasterization into the caller's draw_buf, which is cheaper than holding
 * bitmaps when the owning widget repaints rarely (clocks, mostly-static
 * headings). With the cache disabled, repeated characters in a single
 * label ("00:00") rasterize once per occurrence, not once per unique
 * glyph -- profile the workload before disabling caches for hot strings.
 *
 * Has no effect when `LV_FONT_VEC_CACHE_SIZE == 0` (compile-time disable);
 * the cache field is not present in that build and `cache_size` is ignored.
 *
 * `cache_size` is `uint32_t` because the underlying `lv_cache_t.max_size`
 * is stored as `uint32_t`; passing a wider value would silently truncate.
 *
 * @param font        Font object to populate (caller-owned storage).
 * @param data        Shared outline data (typically const/ROM).
 * @param px          Desired pixel size (line height).
 * @param cache_size  L2 bitmap cache budget in bytes (0 = disabled).
 * @return LV_RESULT_OK on success.
 */
lv_result_t lv_font_vec_init_ex(lv_font_t * font, const lv_font_vec_data_t * data,
                                int32_t px, uint32_t cache_size);

/**
 * Release caches and internal state.  The font must not be used
 * after this call.
 *
 * @param font      Font previously passed to lv_font_vec_init().
 */
void lv_font_vec_deinit(lv_font_t * font);

/**
 * Minimal stub font for LV_FONT_DEFAULT when no bitmap fonts are compiled.
 * Returns placeholders for all glyphs.
 */
extern const lv_font_t lv_font_vec_stub;

/**
 * Log cumulative callback invocation counts (requires LV_USE_LOG).
 */
void lv_font_vec_log_stats(void);

/**
 * Read cumulative L1 metrics-cache hit/miss counters across every live
 * vec font instance. Mirrors `lv_font_vec_get_l2_stats` for the L1
 * (`lv_font_glyph_dsc_t`) lookup path, letting callers gate on the
 * L1 hit rate without scraping log output. Counters are process-wide
 * and free-running for the process lifetime; both arguments may be
 * NULL if the caller only wants one.
 *
 * Available whenever `LV_FONT_VEC_CACHE_L1_SETS > 0` (i.e. the L1
 * cache is compiled in); the runtime cost is one `uint32_t` increment
 * per glyph metrics lookup when the cache is enabled. When the L1
 * cache is compile-time disabled, both outputs are set to 0.
 *
 * @param hits    Out: cumulative hits, or NULL.
 * @param misses  Out: cumulative misses, or NULL.
 */
void lv_font_vec_get_l1_stats(uint32_t * hits, uint32_t * misses);

/**
 * Read cumulative L2 bitmap-cache hit/miss counters across every live
 * vec font instance. Lets callers gate on cache health (e.g. CI tests
 * asserting L2 hit rate stays above a threshold) without scraping log
 * output. Counters are process-wide and free-running for the process
 * lifetime; both arguments may be NULL if the caller only wants one.
 *
 * Available whenever `LV_FONT_VEC_CACHE_SIZE > 0` (i.e. the L2 cache
 * is compiled in); the runtime cost is two `uint32_t` increments per
 * glyph fetch when the cache is enabled. When the L2 cache is
 * compile-time disabled, both outputs are set to 0.
 *
 * @param hits    Out: cumulative hits, or NULL.
 * @param misses  Out: cumulative misses, or NULL.
 */
void lv_font_vec_get_l2_stats(uint32_t * hits, uint32_t * misses);

/**
 * Read the per-instance L2 bitmap-cache utilization for a vec-engine
 * font instance. Returns true when the instance owns a live L2 cache,
 * with `*used` set to the bytes currently held by entries and `*max`
 * to the configured budget; false when the L2 cache is compile-time
 * disabled (`LV_FONT_VEC_CACHE_SIZE == 0`), when the per-instance
 * budget passed to `lv_font_vec_init_ex()` was 0 (cache disabled for
 * that instance), or when `font` is not a vec-engine instance. Either
 * out-pointer may be NULL. Lets callers introspect actual vs.
 * configured byte budgets without reaching into module-private state.
 *
 * @param font  Vec-engine font previously passed to lv_font_vec_init*.
 * @param used  Out: bytes currently held by L2 entries, or NULL.
 * @param max   Out: configured byte budget, or NULL.
 * @return true if the font has a live per-instance L2 cache.
 */
bool lv_font_vec_get_instance_l2_size(const lv_font_t * font,
                                      size_t * used, size_t * max);

/**
 * Read the pixel size baked into a vec-engine font instance at
 * `lv_font_vec_init_ex()` time. Returns 0 when `font` is not a
 * vec-engine instance (lets callers use a non-zero return value as a
 * "this is one of mine" type check without exposing the internal
 * descriptor struct).
 */
int32_t lv_font_vec_get_instance_pixel_size(const lv_font_t * font);

#endif /* LV_USE_FONT_VEC */

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /* LV_FONT_VEC_H */
