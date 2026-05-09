/**
 * @file lv_font_vec.c
 *
 * Vector font engine adapted from mado/libtwin (MIT, Keith Packard).
 * Fixed-point scanline rasterizer: no FPU, no FreeType, no ThorVG.
 */

/*********************
 *      INCLUDES
 *********************/

#include "../../lvgl_public.h"

#if LV_USE_FONT_VEC

#include "../../core/lv_global.h"
#include "../../misc/cache/lv_cache.h"
#include "../../misc/cache/lv_cache_entry.h"
#include "../../misc/cache/class/lv_cache_lru_rb.h"

#include <stdlib.h>  /* qsort */

#define font_draw_buf_handlers &(LV_GLOBAL_DEFAULT()->font_draw_buf_handlers)

/*********************
 *      DEFINES
 *********************/

/*--- Fixed-point types (matching mado) ---*/
typedef int16_t  fv_sfixed_t;   /* Q11.4  -- path coordinates */
typedef int32_t  fv_dfixed_t;   /* Q27.4  -- intermediate products */
typedef int32_t  fv_fixed_t;    /* Q16.16 -- general arithmetic */

#define FV_SFIXED_ONE    0x10
#define FV_SFIXED_MAX    0x7fff
#define FV_SFIXED_MIN    (-0x7fff)

#define fv_sfixed_floor(f)  ((f) & ~0xf)
#define fv_sfixed_trunc(f)  ((f) >> 4)
#define fv_sfixed_ceil(f)   (((f) + 0xf) & ~0xf)
#define fv_int_to_sfixed(i) ((fv_sfixed_t)((i) * 16))

#define FV_GFIXED_ONE    0x40   /* Q1.6 glyph coordinate unit */

/*
 * Scale Q1.6 glyph coord to Q16.16 pixels.
 * g is Q1.6 (signed char, range -2..+1.98).
 * scale is pixel_size << 16 (Q16.16, e.g. 14 << 16 = 917504).
 * Result = g * scale >> 6 = g/64 * pixel_size in Q16.16.
 *
 * Y-axis convention differs by font type:
 *   TTF:    Y-up (positive = ascender) -- caller passes negative scale_y
 *   Stroke: Y-down (negative = ascender, matches screen) -- caller passes positive scale_y
 */
#define FV_SCALE(g, scale) (((int32_t)(g) * (scale)) >> 6)
#define FV_SCALE_X(g, scale_x) FV_SCALE(g, scale_x)
#define FV_SCALE_Y(g, scale_y) FV_SCALE(g, scale_y)

/* Convert Q16.16 to Q11.4 sfixed for path coordinates */
#define fv_fixed_to_sfixed(f) ((fv_sfixed_t)((f) >> 12))

/*--- Rasterizer parameters (4x4 antialiasing) ---*/
#define FV_POLY_SHIFT       2
#define FV_POLY_FIXED_SHIFT (4 - FV_POLY_SHIFT)
#define FV_POLY_SAMPLE      (1 << FV_POLY_SHIFT)
#define FV_POLY_MASK        (FV_POLY_SAMPLE - 1)
#define FV_POLY_STEP        (FV_SFIXED_ONE >> FV_POLY_SHIFT)
#define FV_POLY_START       (FV_POLY_STEP >> 1)

/*--- Spline tolerance ---*/
#define FV_TOLERANCE        (FV_SFIXED_ONE >> 2)
#define FV_TOLERANCE_SQ     ((fv_dfixed_t)FV_TOLERANCE * FV_TOLERANCE)

/*--- Path inline storage ---*/
#define FV_INLINE_POINTS  64
#define FV_INLINE_SUBLEN  4

/*--- Edge stack for rasterizer ---
 * Typical Latin glyphs: 15-30 flattened points -> ~15-30 edges.
 * Complex glyphs (e.g. '@'): up to ~80 points -> ~80 edges.
 * 64 covers all ASCII comfortably; heap fallback for CJK. */
#define FV_EDGE_STACK     64

/*--- A8 saturation ---*/
#define fv_sat(t) ((uint8_t)((t) | (0 - ((t) >> 8))))

/*--- Stroke font glyph header accessors ---*/
#define fv_glyph_left(g)      ((g)[0])
#define fv_glyph_right(g)     ((g)[1])
#define fv_glyph_ascent(g)    ((g)[2])
#define fv_glyph_descent(g)   ((g)[3])
#define fv_glyph_n_snap_x(g)  ((g)[4])
#define fv_glyph_n_snap_y(g)  ((g)[5])
#define fv_glyph_snap_x(g)    (&(g)[6])
#define fv_glyph_snap_y(g)    (fv_glyph_snap_x(g) + fv_glyph_n_snap_x(g))

/*--- Snap macros (from mado font.c) ---*/
#define SNAPI(p) (((p) + 0x8000) & ~0xffff)
#define SNAPH(p) (((p) + 0x4000) & ~0x7fff)

/*--- Fixed-point constants ---*/
#define FV_FIXED_ONE   0x10000
#define FV_FIXED_HALF  0x08000

/*--- Max snap points per glyph ---*/
#define FV_GLYPH_MAX_SNAP_X  4
#define FV_GLYPH_MAX_SNAP_Y  7

/**********************
 *      TYPEDEFS
 **********************/

typedef struct {
    fv_sfixed_t x, y;
} fv_spoint_t;

typedef struct {
    fv_spoint_t * points;
    int           npoints;
    int           size_points;
    int     *     sublen;
    int           nsublen;
    int           size_sublen;
    fv_spoint_t   inline_points[FV_INLINE_POINTS];
    int           inline_sublen[FV_INLINE_SUBLEN];
} fv_path_t;

typedef struct {
    fv_spoint_t a, b, c, d;
} fv_spline_t;

typedef struct fv_edge {
    struct fv_edge * next;
    fv_sfixed_t top, bot;
    fv_sfixed_t x;
    fv_sfixed_t e;
    fv_sfixed_t dx, dy;
    fv_sfixed_t inc_x;
    fv_sfixed_t step_x;
    int16_t     winding;    /* +1 or -1 only; int16 eliminates padding */
} fv_edge_t;

/** Lightweight A8 render target (replaces twin_pixmap_t). */
typedef struct {
    uint8_t * buf;
    int32_t   width;
    int32_t   height;
    int32_t   stride;
} fv_a8_target_t;

/*--------------------------------------------------------------------
 * L2 bitmap cache node (for lv_cache_class_lru_rb_size)
 *--------------------------------------------------------------------*/

#if LV_FONT_VEC_CACHE_SIZE > 0

typedef struct {
    lv_cache_slot_size_t slot;       /* MUST be first -- byte cost for LRU eviction */
    uint32_t             unicode;
    lv_draw_buf_t    *   draw_buf;   /* A8 bitmap, owned by cache */
    uint16_t             box_w;      /* glyph box width (avoids re-walking outline) */
    uint16_t             box_h;      /* glyph box height */
    int16_t              ofs_x;      /* X offset from origin */
    int16_t              ofs_y;      /* Y offset (bbox top, screen coords) */
} lv_font_vec_bitmap_node_t;

#endif /* LV_FONT_VEC_CACHE_SIZE > 0 */

/* Per-instance descriptor stored in lv_font_t.dsc. Allocated by
 * lv_font_vec_init_ex; the L1 metrics cache is a process-wide shared
 * table maintained at the bottom of this file. */
typedef struct {
    const lv_font_vec_data_t   *  data;
    const lv_font_vec_charmap_t * cur_page;
    int32_t                       pixel_size;

#if LV_FONT_VEC_CACHE_SIZE > 0
    lv_cache_t          *         bitmap_cache;
#endif
} lv_font_vec_dsc_t;

/**********************
 *  STATIC PROTOTYPES
 **********************/

/* LVGL font callbacks */
static bool     vec_get_glyph_dsc_cb(const lv_font_t * font, lv_font_glyph_dsc_t * dsc_out,
                                     uint32_t letter, uint32_t letter_next);
static const void * vec_get_glyph_bitmap_cb(lv_font_glyph_dsc_t * g_dsc, lv_draw_buf_t * draw_buf);
static void     vec_release_glyph_cb(const lv_font_t * font, lv_font_glyph_dsc_t * g_dsc);

/* Glyph lookup */
static bool           fv_find_page(lv_font_vec_dsc_t * dsc, uint32_t page);
static const int8_t * fv_glyph_base(lv_font_vec_dsc_t * dsc, uint32_t ucs4);

/* Outline bbox computation (no path construction) */
static void fv_outline_bbox(const int8_t * base, const int8_t * end,
                            fv_fixed_t scale_x, fv_fixed_t scale_y,
                            fv_fixed_t * left, fv_fixed_t * top,
                            fv_fixed_t * right, fv_fixed_t * bottom);

/* Outline-to-path parser */
static void fv_outline_to_path(fv_path_t * path, const int8_t * base, const int8_t * end,
                               fv_fixed_t scale_x, fv_fixed_t scale_y,
                               uint8_t font_type,
                               const fv_fixed_t * snap_x, int n_snap_x,
                               const fv_fixed_t * snap_y, int n_snap_y);

/* Path operations */
static void fv_path_init(fv_path_t * path);
static void fv_path_cleanup(fv_path_t * path);
static void fv_path_sdraw(fv_path_t * path, fv_sfixed_t x, fv_sfixed_t y);
static void fv_path_smove(fv_path_t * path, fv_sfixed_t x, fv_sfixed_t y);
static void fv_path_sfinish(fv_path_t * path);

/* Spline */
static void fv_path_scurve(fv_path_t * path,
                           fv_sfixed_t x1, fv_sfixed_t y1,
                           fv_sfixed_t x2, fv_sfixed_t y2,
                           fv_sfixed_t x3, fv_sfixed_t y3);

/* Rasterizer */
static void fv_fill_path(fv_a8_target_t * target, fv_path_t * path,
                         fv_sfixed_t dx, fv_sfixed_t dy);

/* Rasterize into draw_buf */
static bool fv_rasterize_glyph(lv_font_vec_dsc_t * dsc, const int8_t * base,
                               uint16_t box_w, uint16_t box_h,
                               int16_t ofs_x, int16_t ofs_y,
                               lv_draw_buf_t * draw_buf);

/* L1 cache helpers (process-wide shared table, keyed on (data, pixel_size, unicode)) */
#if LV_FONT_VEC_CACHE_L1_SETS > 0
/* The set index is derived as `hash & (LV_FONT_VEC_CACHE_L1_SETS - 1)`, so
 * the value must be a power of two; a non-power-of-two would silently
 * underuse the table and collide unrelated keys onto the same slot. */
#if (LV_FONT_VEC_CACHE_L1_SETS & (LV_FONT_VEC_CACHE_L1_SETS - 1)) != 0
    #error "LV_FONT_VEC_CACHE_L1_SETS must be a power of two (or 0 to disable)"
#endif
typedef struct {
    const void     *     data;       /* lv_font_vec_data_t * (typeface) */
    int32_t              pixel_size; /* render size disambiguator       */
    uint32_t             unicode;
    lv_font_glyph_dsc_t  dsc;
} fv_l1_entry_t;

typedef struct {
    fv_l1_entry_t ways[2];   /* 2-way set-associative */
    uint8_t       lru_bits;  /* index of last-accessed way */
} fv_l1_set_t;

static uint32_t fv_l1_hash(const void * data, int32_t pixel_size, uint32_t unicode);
static bool     fv_l1_lookup(lv_font_vec_dsc_t * dsc, uint32_t unicode, lv_font_glyph_dsc_t * out);
static void     fv_l1_fill(lv_font_vec_dsc_t * dsc, uint32_t unicode, const lv_font_glyph_dsc_t * d);
static void     fv_l1_acquire(void);
static void     fv_l1_release(void);
static void     fv_l1_invalidate(const void * data, int32_t pixel_size);
#endif

/* L2 cache callbacks */
#if LV_FONT_VEC_CACHE_SIZE > 0
static bool     fv_bitmap_create_cb(lv_font_vec_bitmap_node_t * node, void * user_data);
static void     fv_bitmap_free_cb(lv_font_vec_bitmap_node_t * node, void * user_data);
static lv_cache_compare_res_t fv_bitmap_compare_cb(const lv_font_vec_bitmap_node_t * a,
                                                   const lv_font_vec_bitmap_node_t * b);
#endif

/**********************
 *  STATIC VARIABLES
 **********************/

#if LV_USE_LOG
    static uint32_t s_vec_dsc_calls = 0;
    static uint32_t s_vec_bitmap_calls = 0;
#endif

#if LV_FONT_VEC_CACHE_L1_SETS > 0
    /* Process-wide shared L1 metrics cache.  One table, ~3.8 KiB, replaces
    * the per-instance allocation -- savings scale linearly with the number
    * of live font instances (5 fonts -> ~15 KiB heap saved).
    *
    * Hit/miss counters are not gated on LV_USE_LOG -- they back the public
    * `lv_font_vec_get_l1_stats` accessor, which a CI/test harness needs
    * to run independently of the log build flag. */
    static fv_l1_set_t * s_l1_cache;
    static uint32_t      s_l1_refs;
    static uint32_t      s_l1_hits;
    static uint32_t      s_l1_misses;
#endif

#if LV_FONT_VEC_CACHE_SIZE > 0
    /* L2 bitmap cache hit/miss tally. The `vec_get_glyph_bitmap_cb` counter
    * (s_vec_bitmap_calls) increments before the cache lookup and reflects
    * glyph *demand*, not cache outcome. These two count the actual L2 path:
    * `s_l2_hits` increments only when `lv_cache_acquire_or_create` returns
    * a pre-existing entry; `s_l2_misses` increments when the same call
    * had to invoke `fv_bitmap_create_cb` to materialize a fresh node.
    * Distinguishing them is the only way to detect whether a tighter
    * cache budget is causing per-frame re-rasterization on revisited
    * screens, which the demand counter alone hides. Counters are not
    * gated on LV_USE_LOG -- they back the public `lv_font_vec_get_l2_stats`
    * accessor, which a CI/test harness needs to run independently of the
    * log build flag. The runtime cost is two uint32_t increments per
    * glyph fetch when the cache is enabled.
    *
    * `s_l2_create_invoked` is the side-channel: `fv_bitmap_create_cb`
    * sets it to true; `vec_get_glyph_bitmap_cb` clears it before the
    * acquire_or_create call and reads it after. Single-threaded only --
    * LVGL on LV_OS_NONE has no preempting thread that could race. */
    static uint32_t      s_l2_hits;
    static uint32_t      s_l2_misses;
    static bool          s_l2_create_invoked;
#endif

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

lv_result_t lv_font_vec_init(lv_font_t * font, const lv_font_vec_data_t * data, int32_t px)
{
    return lv_font_vec_init_ex(font, data, px, LV_FONT_VEC_CACHE_SIZE);
}

lv_result_t lv_font_vec_init_ex(lv_font_t * font, const lv_font_vec_data_t * data,
                                int32_t px, uint32_t cache_size)
{
    LV_ASSERT_NULL(font);
    LV_ASSERT_NULL(data);
    /*
     * FV_SCALE_X/Y: (int8_t)g * (px << 16) >> 6.
     * Intermediate is int32_t; overflow at |g|*px*1024 > 2^31.
     * g range: -128..+127. Safe limit: px <= 2^31 / (128*1024) = 16384.
     * Clamp to 256 which is generous for embedded targets.
     */
    if(px <= 0 || px > 256) return LV_RESULT_INVALID;
#if LV_FONT_VEC_CACHE_SIZE == 0
    LV_UNUSED(cache_size);
#endif

    lv_font_vec_dsc_t * dsc = lv_malloc_zeroed(sizeof(lv_font_vec_dsc_t));
    LV_ASSERT_MALLOC(dsc);
    if(!dsc) return LV_RESULT_INVALID;

    dsc->data       = data;
    dsc->cur_page   = NULL;
    dsc->pixel_size = px;

    /* Compute font metrics from Q1.6 ascender/descender/height.
     * For stroke fonts, the pen convolution expands glyphs by pen_radius
     * on each side.  Add 2 * pen_radius to the line height and ascender
     * so LVGL allocates enough vertical space for rendered glyphs. */
    /* Use fixed-point scale then shift to avoid C truncation-toward-zero
     * which loses fractional descent (e.g. -168/64 = -2 in C, not -3). */
    fv_fixed_t metric_scale = (fv_fixed_t)px << 16;
    int32_t ascender  = (int32_t)((FV_SCALE(data->ascender, metric_scale) + 0xffff) >> 16);  /* ceil */
    int32_t descender = (int32_t)(FV_SCALE(data->descender, metric_scale) >> 16);              /* floor */
    int32_t height    = (int32_t)((FV_SCALE(data->height, metric_scale) + 0xffff) >> 16);     /* ceil */

    if(data->type == LV_FONT_VEC_TYPE_STROKE) {
        fv_fixed_t scale = (fv_fixed_t)px << 16;
        fv_fixed_t pen_w = SNAPH(scale / 24);
        if(pen_w < FV_FIXED_HALF) pen_w = FV_FIXED_HALF;
        int32_t pen_px = (int32_t)((pen_w + FV_FIXED_HALF) >> 16);
        /* Pen convolution expands glyphs by pen_radius on each side.
         * The bbox adds (pen_w + 1px) safety margin. Match that here. */
        int32_t pad_px = pen_px + 1;
        ascender  += pad_px;
        if(descender < 0) descender -= pad_px;
        height    += pad_px * 2;
    }

    /* L2 bitmap cache: per-instance budget. cache_size = 0 disables the
     * cache for this instance (vec_get_glyph_bitmap_cb falls back to
     * rasterizing on every call). */
#if LV_FONT_VEC_CACHE_SIZE > 0
    if(cache_size > 0) {
        lv_cache_ops_t ops = {
            .create_cb  = (lv_cache_create_cb_t)fv_bitmap_create_cb,
            .free_cb    = (lv_cache_free_cb_t)fv_bitmap_free_cb,
            .compare_cb = (lv_cache_compare_cb_t)fv_bitmap_compare_cb,
        };
        dsc->bitmap_cache = lv_cache_create(&lv_cache_class_lru_rb_size,
                                            sizeof(lv_font_vec_bitmap_node_t),
                                            cache_size, ops);
        if(dsc->bitmap_cache) {
            lv_cache_set_name(dsc->bitmap_cache, "FONT_VEC");
        }
    }
#endif

    /* L1 metrics cache: shared across all live vec font instances. */
#if LV_FONT_VEC_CACHE_L1_SETS > 0
    fv_l1_acquire();
#endif

    /* Populate lv_font_t */
    lv_memzero(font, sizeof(lv_font_t));
    font->dsc               = dsc;
    font->get_glyph_dsc     = vec_get_glyph_dsc_cb;
    font->get_glyph_bitmap  = vec_get_glyph_bitmap_cb;
    font->release_glyph     = vec_release_glyph_cb;
    font->line_height        = height > 0 ? height : px;
    font->base_line          = descender < 0 ? -descender : 0;
    font->subpx              = 0;
    font->kerning            = LV_FONT_KERNING_NONE;
    font->static_bitmap      = 0;
    font->underline_position  = -1;
    font->underline_thickness = 1;

    /* Approximate cap_height and x_height from ascender */
    font->cap_height = ascender > 0 ? ascender : (px * 7 / 10);
    font->x_height   = font->cap_height * 5 / 7;

    return LV_RESULT_OK;
}

void lv_font_vec_log_stats(void)
{
#if LV_USE_LOG
    LV_LOG_USER("vec font stats: get_glyph_dsc=%" LV_PRIu32 " get_glyph_bitmap=%" LV_PRIu32,
                s_vec_dsc_calls, s_vec_bitmap_calls);
#if LV_FONT_VEC_CACHE_L1_SETS > 0
    /* Widen the denominator to 64 bits.  Both counters are uint32 and
     * free-running for the process lifetime; on a long-lived embedded
     * device the sum can wrap to a small value and produce a nonsense
     * percentage.  Computing the sum in 64 bits keeps the ratio sane
     * past the 32-bit boundary without requiring 64-bit counters. */
    uint64_t l1_total = (uint64_t)s_l1_hits + (uint64_t)s_l1_misses;
    uint32_t l1_pct   = l1_total ? (uint32_t)((uint64_t)s_l1_hits * 1000u / l1_total) : 0u;
    LV_LOG_USER("vec font L1 cache: hits=%" LV_PRIu32 " misses=%" LV_PRIu32
                " hit_rate=%" LV_PRIu32 ".%" LV_PRIu32 "%%",
                s_l1_hits, s_l1_misses, l1_pct / 10u, l1_pct % 10u);
#endif
#if LV_FONT_VEC_CACHE_SIZE > 0
    uint64_t l2_total = (uint64_t)s_l2_hits + (uint64_t)s_l2_misses;
    uint32_t l2_pct   = l2_total ? (uint32_t)((uint64_t)s_l2_hits * 1000u / l2_total) : 0u;
    LV_LOG_USER("vec font L2 cache: hits=%" LV_PRIu32 " misses=%" LV_PRIu32
                " hit_rate=%" LV_PRIu32 ".%" LV_PRIu32 "%%",
                s_l2_hits, s_l2_misses, l2_pct / 10u, l2_pct % 10u);
#endif
#endif
}

void lv_font_vec_get_l1_stats(uint32_t * hits, uint32_t * misses)
{
#if LV_FONT_VEC_CACHE_L1_SETS > 0
    if(hits)   *hits   = s_l1_hits;
    if(misses) *misses = s_l1_misses;
#else
    if(hits)   *hits   = 0;
    if(misses) *misses = 0;
#endif
}

void lv_font_vec_get_l2_stats(uint32_t * hits, uint32_t * misses)
{
#if LV_FONT_VEC_CACHE_SIZE > 0
    if(hits)   *hits   = s_l2_hits;
    if(misses) *misses = s_l2_misses;
#else
    if(hits)   *hits   = 0;
    if(misses) *misses = 0;
#endif
}

/* `font->get_glyph_dsc == vec_get_glyph_dsc_cb` is the only safe way
 * for callers to know `font->dsc` points at our private descriptor.
 * Comparing against the file-static callback pointer keeps the
 * descriptor type private while still letting external code identify
 * a vec-engine instance without an explicit "is_vec" flag in
 * `lv_font_t`. */
static const lv_font_vec_dsc_t * fv_dsc_of(const lv_font_t * font)
{
    if(!font || font->get_glyph_dsc != vec_get_glyph_dsc_cb) return NULL;
    return (const lv_font_vec_dsc_t *)font->dsc;
}

bool lv_font_vec_get_instance_l2_size(const lv_font_t * font,
                                      size_t * used, size_t * max)
{
    if(used) *used = 0;
    if(max)  *max  = 0;

#if LV_FONT_VEC_CACHE_SIZE > 0
    const lv_font_vec_dsc_t * dsc = fv_dsc_of(font);
    if(!dsc || !dsc->bitmap_cache) return false;
    if(used) *used = lv_cache_get_size(dsc->bitmap_cache, NULL);
    if(max)  *max  = lv_cache_get_max_size(dsc->bitmap_cache, NULL);
    return true;
#else
    LV_UNUSED(font);
    return false;
#endif
}

int32_t lv_font_vec_get_instance_pixel_size(const lv_font_t * font)
{
    const lv_font_vec_dsc_t * dsc = fv_dsc_of(font);
    return dsc ? dsc->pixel_size : 0;
}

/*
 * Minimal stub font for LV_FONT_DEFAULT when no bitmap fonts are compiled.
 * Returns placeholder for every glyph. The demo overrides all theme font
 * slots with vec font instances before any text is actually rendered.
 */
static bool stub_get_glyph_dsc(const lv_font_t * font, lv_font_glyph_dsc_t * dsc,
                               uint32_t letter, uint32_t letter_next)
{
    LV_UNUSED(font);
    LV_UNUSED(letter_next);
    dsc->adv_w = letter >= 0x20 ? 8 : 0;
    dsc->box_w = 0;
    dsc->box_h = 0;
    dsc->ofs_x = 0;
    dsc->ofs_y = 0;
    dsc->format = LV_FONT_GLYPH_FORMAT_NONE;
    dsc->is_placeholder = 1;
    return true;
}

const lv_font_t lv_font_vec_stub = {
    .get_glyph_dsc = stub_get_glyph_dsc,
    .get_glyph_bitmap = NULL,
    .release_glyph = NULL,
    .line_height = 14,
    .base_line = 2,
};

void lv_font_vec_deinit(lv_font_t * font)
{
    if(!font || !font->dsc) return;
    lv_font_vec_dsc_t * dsc = (lv_font_vec_dsc_t *)font->dsc;

#if LV_FONT_VEC_CACHE_SIZE > 0
    if(dsc->bitmap_cache) {
        lv_cache_destroy(dsc->bitmap_cache, NULL);
        dsc->bitmap_cache = NULL;
    }
#endif

#if LV_FONT_VEC_CACHE_L1_SETS > 0
    /* Drop only the entries that belonged to *this* instance; sibling
     * instances sharing `dsc->data` at other pixel sizes stay live.
     * This guards against an `lv_font_vec_data_t` being heap-allocated
     * and reused at the same address by a future font: without the
     * purge, the new font would receive stale glyph metrics on lookup. */
    fv_l1_invalidate(dsc->data, dsc->pixel_size);
    fv_l1_release();
#endif

    lv_free(dsc);
    font->dsc = NULL;
}

/**********************
 *   GLYPH LOOKUP
 **********************/

static inline uint32_t fv_ucs_page(uint32_t ucs4)
{
    return ucs4 >> LV_FONT_VEC_PAGE_SHIFT;
}

static inline uint32_t fv_ucs_char_in_page(uint32_t ucs4)
{
    return ucs4 & (LV_FONT_VEC_PER_PAGE - 1);
}

static bool fv_find_page(lv_font_vec_dsc_t * dsc, uint32_t page)
{
    const lv_font_vec_data_t * data = dsc->data;

    if(dsc->cur_page && dsc->cur_page->page == page)
        return true;

    for(int32_t i = 0; i < data->n_charmap; i++) {
        if(data->charmap[i].page == page) {
            dsc->cur_page = &data->charmap[i];
            return true;
        }
    }

    if(data->n_charmap > 0)
        dsc->cur_page = &data->charmap[0];
    return false;
}

static const int8_t * fv_glyph_base(lv_font_vec_dsc_t * dsc, uint32_t ucs4)
{
    uint32_t idx = fv_ucs_char_in_page(ucs4);
    if(!fv_find_page(dsc, fv_ucs_page(ucs4)))
        idx = 0;

    if(!dsc->cur_page) return NULL; /* empty font or corrupt data */

    uint32_t offset = dsc->cur_page->offsets[idx];
    if(offset == 0) return NULL; /* glyph not present */
    if(dsc->data->outlines_size > 0 && offset >= dsc->data->outlines_size)
        return NULL; /* offset out of bounds */

    return dsc->data->outlines + offset;
}

/**********************
 *  OUTLINE BBOX
 **********************/

/**
 * Walk outline commands to compute bounding box without building a path.
 * For TTF outlines, the glyph data starts at base[0] = advance (1 byte),
 * then drawing commands follow from base[1].
 */
static void fv_outline_bbox(const int8_t * base, const int8_t * end,
                            fv_fixed_t scale_x, fv_fixed_t scale_y,
                            fv_fixed_t * left, fv_fixed_t * top,
                            fv_fixed_t * right, fv_fixed_t * bottom)
{
    fv_fixed_t min_x = 0x7fffffff, min_y = 0x7fffffff;
    fv_fixed_t max_x = -0x7fffffff, max_y = -0x7fffffff;

    const int8_t * g = base + 1; /* skip advance byte */

    for(;;) {
        if(g >= end) break;
        int8_t op = *g++;
        fv_fixed_t x, y;
        switch(op) {
            case 'm': /* moveto: 2 coords */
            case 'l': /* lineto: 2 coords */
                if(g + 2 > end) goto done;
                x = FV_SCALE_X(*g++, scale_x);
                y = FV_SCALE_Y(*g++, scale_y);
                if(x < min_x) min_x = x;
                if(x > max_x) max_x = x;
                if(y < min_y) min_y = y;
                if(y > max_y) max_y = y;
                break;
            case '2': { /* quadratic: control + endpoint (4 bytes) */
                    if(g + 4 > end) goto done;
                    fv_fixed_t cx = FV_SCALE_X(*g++, scale_x);
                    fv_fixed_t cy = FV_SCALE_Y(*g++, scale_y);
                    x = FV_SCALE_X(*g++, scale_x);
                    y = FV_SCALE_Y(*g++, scale_y);
                    if(cx < min_x) min_x = cx;
                    if(cx > max_x) max_x = cx;
                    if(cy < min_y) min_y = cy;
                    if(cy > max_y) max_y = cy;
                    if(x < min_x) min_x = x;
                    if(x > max_x) max_x = x;
                    if(y < min_y) min_y = y;
                    if(y > max_y) max_y = y;
                    break;
                }
            case 'c': { /* cubic: 3 points (6 bytes) */
                    if(g + 6 > end) goto done;
                    for(int i = 0; i < 3; i++) {
                        x = FV_SCALE_X(*g++, scale_x);
                        y = FV_SCALE_Y(*g++, scale_y);
                        if(x < min_x) min_x = x;
                        if(x > max_x) max_x = x;
                        if(y < min_y) min_y = y;
                        if(y > max_y) max_y = y;
                    }
                    break;
                }
            case 'e': /* end */
                goto done;
            default:
                goto done;
        }
    }
done:
    if(min_x > max_x) {
        min_x = max_x = min_y = max_y = 0;
    }
    *left   = min_x;
    *top    = min_y;
    *right  = max_x;
    *bottom = max_y;
}

/**********************
 *     PATH OPS
 **********************/

static void fv_path_init(fv_path_t * path)
{
    path->points      = path->inline_points;
    path->size_points  = FV_INLINE_POINTS;
    path->npoints      = 0;
    path->sublen       = path->inline_sublen;
    path->size_sublen  = FV_INLINE_SUBLEN;
    path->nsublen      = 0;
}

static void fv_path_cleanup(fv_path_t * path)
{
    if(path->points != path->inline_points)
        lv_free(path->points);
    if(path->sublen != path->inline_sublen)
        lv_free(path->sublen);
}

static int fv_current_subpath_len(fv_path_t * path)
{
    if(path->nsublen)
        return path->npoints - path->sublen[path->nsublen - 1];
    return path->npoints;
}

static void fv_path_sdraw(fv_path_t * path, fv_sfixed_t x, fv_sfixed_t y)
{
    /* Deduplicate */
    if(fv_current_subpath_len(path) > 0 &&
       path->points[path->npoints - 1].x == x &&
       path->points[path->npoints - 1].y == y)
        return;

    /* Grow if needed */
    if(path->npoints == path->size_points) {
        int new_size = path->size_points * 2;
        fv_spoint_t * pts;
        if(path->points == path->inline_points) {
            pts = lv_malloc((size_t)new_size * sizeof(fv_spoint_t));
            if(!pts) return;
            lv_memcpy(pts, path->inline_points,
                      (size_t)path->npoints * sizeof(fv_spoint_t));
        }
        else {
            pts = lv_realloc(path->points, (size_t)new_size * sizeof(fv_spoint_t));
            if(!pts) return;
        }
        path->points = pts;
        path->size_points = new_size;
    }
    path->points[path->npoints].x = x;
    path->points[path->npoints].y = y;
    path->npoints++;
}

static void fv_path_sfinish(fv_path_t * path)
{
    int len = fv_current_subpath_len(path);
    if(len <= 1) {
        if(len == 1) path->npoints--;
        return;
    }
    if(path->nsublen == path->size_sublen) {
        int new_size = path->size_sublen * 2;
        int * sl;
        if(path->sublen == path->inline_sublen) {
            sl = lv_malloc((size_t)new_size * sizeof(int));
            if(!sl) return;
            lv_memcpy(sl, path->inline_sublen,
                      (size_t)path->nsublen * sizeof(int));
        }
        else {
            sl = lv_realloc(path->sublen, (size_t)new_size * sizeof(int));
            if(!sl) return;
        }
        path->sublen = sl;
        path->size_sublen = new_size;
    }
    path->sublen[path->nsublen] = path->npoints;
    path->nsublen++;
}

static void fv_path_smove(fv_path_t * path, fv_sfixed_t x, fv_sfixed_t y)
{
    int len = fv_current_subpath_len(path);
    if(len > 1) {
        fv_path_sfinish(path);
        fv_path_sdraw(path, x, y);
    }
    else if(len == 1) {
        path->points[path->npoints - 1].x = x;
        path->points[path->npoints - 1].y = y;
    }
    else {
        fv_path_sdraw(path, x, y);
    }
}

/**********************
 *      SPLINE
 **********************/

static fv_dfixed_t fv_distance_to_point_sq(const fv_spoint_t * a, const fv_spoint_t * b)
{
    fv_dfixed_t dx = b->x - a->x;
    fv_dfixed_t dy = b->y - a->y;
    return dx * dx + dy * dy;
}

static fv_dfixed_t fv_distance_to_line_sq(fv_spoint_t * p, fv_spoint_t * p1, fv_spoint_t * p2)
{
    fv_dfixed_t A = p2->y - p1->y;
    fv_dfixed_t B = p1->x - p2->x;
    fv_dfixed_t C = (fv_dfixed_t)p1->y * p2->x - (fv_dfixed_t)p1->x * p2->y;
    fv_dfixed_t num = A * p->x + B * p->y + C;
    fv_dfixed_t den;
    if(num < 0) num = -num;
    den = A * A + B * B;
    if(den == 0 || num >= 0x8000)
        return fv_distance_to_point_sq(p, p1);
    return (num * num) / den;
}

static fv_dfixed_t fv_spline_dist_sq(fv_spline_t * s)
{
    fv_dfixed_t bd = fv_distance_to_line_sq(&s->b, &s->a, &s->d);
    fv_dfixed_t cd = fv_distance_to_line_sq(&s->c, &s->a, &s->d);
    return bd > cd ? bd : cd;
}

static void fv_lerp(const fv_spoint_t * a, const fv_spoint_t * b, int shift, fv_spoint_t * r)
{
    r->x = a->x + ((b->x - a->x) >> shift);
    r->y = a->y + ((b->y - a->y) >> shift);
}

static void fv_de_casteljau(fv_spline_t * sp, int shift, fv_spline_t * s1, fv_spline_t * s2)
{
    fv_spoint_t ab, bc, cd, abbc, bccd, final;
    fv_lerp(&sp->a, &sp->b, shift, &ab);
    fv_lerp(&sp->b, &sp->c, shift, &bc);
    fv_lerp(&sp->c, &sp->d, shift, &cd);
    fv_lerp(&ab, &bc, shift, &abbc);
    fv_lerp(&bc, &cd, shift, &bccd);
    fv_lerp(&abbc, &bccd, shift, &final);

    s1->a = sp->a;
    s1->b = ab;
    s1->c = abbc;
    s1->d = final;
    s2->a = final;
    s2->b = bccd;
    s2->c = cd;
    s2->d = sp->d;
}

static void fv_spline_decompose(fv_path_t * path, fv_spline_t * sp)
{
    fv_path_sdraw(path, sp->a.x, sp->a.y);
    int shift = 2;
    while(fv_spline_dist_sq(sp) > FV_TOLERANCE_SQ) {
        fv_spline_t left, right;
        for(;;) {
            fv_de_casteljau(sp, shift, &left, &right);
            if(fv_spline_dist_sq(&left) <= FV_TOLERANCE_SQ) {
                if(shift > 1) shift--;
                break;
            }
            shift++;
        }
        fv_path_sdraw(path, left.d.x, left.d.y);
        lv_memcpy(sp, &right, sizeof(fv_spline_t));
    }
    fv_path_sdraw(path, sp->d.x, sp->d.y);
}

static void fv_path_scurve(fv_path_t * path,
                           fv_sfixed_t x1, fv_sfixed_t y1,
                           fv_sfixed_t x2, fv_sfixed_t y2,
                           fv_sfixed_t x3, fv_sfixed_t y3)
{
    if(path->npoints == 0)
        fv_path_smove(path, 0, 0);

    fv_spline_t sp = {
        .a = path->points[path->npoints - 1],
        .b = {x1, y1},
        .c = {x2, y2},
        .d = {x3, y3},
    };
    fv_spline_decompose(path, &sp);
}

/**********************
 *  CONVEX HULL (adapted from mado hull.c, Graham scan)
 **********************/

typedef struct {
    fv_sfixed_t dx, dy;
} fv_slope_t;

typedef struct {
    fv_spoint_t point;
    fv_slope_t  slope;
    bool        discard;
} fv_hull_t;

static void fv_slope_init(fv_slope_t * s, const fv_spoint_t * a, const fv_spoint_t * b)
{
    s->dx = b->x - a->x;
    s->dy = b->y - a->y;
}

static int fv_slope_compare(const fv_slope_t * a, const fv_slope_t * b)
{
    fv_dfixed_t diff = (fv_dfixed_t)a->dy * b->dx - (fv_dfixed_t)b->dy * a->dx;
    if(diff > 0) return 1;
    if(diff < 0) return -1;
    if(a->dx == 0 && a->dy == 0) return 1;
    if(b->dx == 0 && b->dy == 0) return -1;
    return 0;
}

static int fv_hull_vertex_compare(const void * av, const void * bv)
{
    fv_hull_t * a = (fv_hull_t *)av;
    fv_hull_t * b = (fv_hull_t *)bv;
    int ret = fv_slope_compare(&a->slope, &b->slope);
    if(ret == 0) {
        fv_dfixed_t ad = (fv_dfixed_t)a->slope.dx * a->slope.dx +
                         (fv_dfixed_t)a->slope.dy * a->slope.dy;
        fv_dfixed_t bd = (fv_dfixed_t)b->slope.dx * b->slope.dx +
                         (fv_dfixed_t)b->slope.dy * b->slope.dy;
        if(ad < bd) {
            a->discard = true;
            ret = -1;
        }
        else        {
            b->discard = true;
            ret = 1;
        }
    }
    return ret;
}

static void fv_hull_eliminate_concave(fv_hull_t * hull, int n)
{
    fv_slope_t sij, sjk;
    int i = 0;
    int j, k;

    /* find first valid after 0 */
    for(j = 1; j < n && hull[j].discard; j++);
    if(j >= n) return;
    for(k = j + 1; k < n && hull[k].discard; k++);
    if(k >= n) k = 0;

    do {
        fv_slope_init(&sij, &hull[i].point, &hull[j].point);
        fv_slope_init(&sjk, &hull[j].point, &hull[k].point);
        if(fv_slope_compare(&sij, &sjk) >= 0) {
            if(i == k) break;
            hull[j].discard = true;
            j = i;
            /* find prev valid */
            if(j > 0) {
                for(j--; j > 0 && hull[j].discard; j--);
            }
            i = j;
            if(i > 0) {
                for(i--; i > 0 && hull[i].discard; i--);
            }
        }
        else {
            i = j;
            j = k;
            for(k = j + 1; k < n && hull[k].discard; k++);
            if(k >= n) k = 0;
        }
    } while(j != 0);
}

/**
 * Compute convex hull of a path. Returns a new path (caller must cleanup).
 * Returns NULL on allocation failure.
 */
static bool fv_path_convex_hull(fv_path_t * src, fv_path_t * dst)
{
    int n = src->npoints;
    if(n < 2) return false;

    /* Find extremal point (lowest-leftmost) */
    int e = 0;
    for(int i = 1; i < n; i++)
        if(src->points[i].y < src->points[e].y ||
           (src->points[i].y == src->points[e].y && src->points[i].x < src->points[e].x))
            e = i;

    /* Allocate hull array on heap (pen circles are small, typically 8-16 points) */
    fv_hull_t * hull = lv_malloc((size_t)n * sizeof(fv_hull_t));
    if(!hull) return false;

    for(int i = 0; i < n; i++) {
        int j = (i == 0) ? e : (i == e) ? 0 : i;
        hull[i].point = src->points[j];
        fv_slope_init(&hull[i].slope, &hull[0].point, &hull[i].point);
        hull[i].discard = (i != 0 && hull[i].slope.dx == 0 && hull[i].slope.dy == 0);
    }

    if(n > 2)
        qsort(hull + 1, (size_t)(n - 1), sizeof(fv_hull_t), fv_hull_vertex_compare);

    fv_hull_eliminate_concave(hull, n);

    /* Build result path */
    fv_path_init(dst);
    for(int i = 0; i < n; i++)
        if(!hull[i].discard)
            fv_path_sdraw(dst, hull[i].point.x, hull[i].point.y);

    lv_free(hull);
    return true;
}

/**********************
 *  STROKE CONVOLUTION (adapted from mado convolve.c, Minkowski sum)
 **********************/

/** Find the point in path furthest left of line p1->p2 */
static int fv_path_leftpoint(fv_path_t * path, const fv_spoint_t * p1, const fv_spoint_t * p2)
{
    fv_dfixed_t Ap = p2->y - p1->y;
    fv_dfixed_t Bp = p1->x - p2->x;
    fv_dfixed_t max = -0x7fffffff;
    int best = 0;
    for(int i = 0; i < path->npoints; i++) {
        fv_dfixed_t vp = Ap * path->points[i].x + Bp * path->points[i].y;
        if(vp > max) {
            max = vp;
            best = i;
        }
    }
    return best;
}

/** Cross product sign to determine which edge to step along */
static int fv_around_order(const fv_spoint_t * a1, const fv_spoint_t * a2,
                           const fv_spoint_t * b1, const fv_spoint_t * b2)
{
    fv_dfixed_t diff = (fv_dfixed_t)(a2->y - a1->y) * (b2->x - b1->x) -
                       (fv_dfixed_t)(b2->y - b1->y) * (a2->x - a1->x);
    if(diff < 0) return -1;
    if(diff > 0) return 1;
    return 0;
}

/** Convolve one subpath with convex pen. Appends closed polygon to result. */
static void fv_subpath_convolve(fv_path_t * result,
                                fv_spoint_t * sp, int ns,
                                fv_path_t * pen)
{
    fv_spoint_t * pp = pen->points;
    int np = pen->npoints;
    if(ns < 2 || np < 2) return;

    int start = fv_path_leftpoint(pen, &sp[0], &sp[1]);
    int ret   = fv_path_leftpoint(pen, &sp[ns - 1], &sp[ns - 2]);

    int s = 0, p = start;
    fv_path_smove(result, sp[s].x + pp[p].x, sp[s].y + pp[p].y);

    int inc = 1, starget = ns - 1, ptarget = ret;

    for(;;) {
        /* Convolve edges: step along stroke or pen */
        do {
            int sn = s + inc;
            int pn = (p == np - 1) ? 0 : p + 1;
            int pm = (p == 0) ? np - 1 : p - 1;

            if(fv_around_order(&sp[s], &sp[sn], &pp[p], &pp[pn]) > 0)
                p = pn;
            else if(fv_around_order(&sp[s], &sp[sn], &pp[pm], &pp[p]) < 0)
                p = pm;
            else
                s = sn;
            fv_path_sdraw(result, sp[s].x + pp[p].x, sp[s].y + pp[p].y);
        } while(s != starget);

        /* Round cap at endpoint: walk pen boundary */
        while(p != ptarget) {
            if(++p == np) p = 0;
            fv_path_sdraw(result, sp[s].x + pp[p].x, sp[s].y + pp[p].y);
        }

        if(inc == -1) break;

        /* Reverse direction for return stroke */
        inc = -1;
        ptarget = start;
        starget = 0;
    }

    /* Close the convolved polygon */
    fv_path_sfinish(result);
}

/** Convolve all subpaths in stroke with pen. Result is filled polygon. */
static void fv_path_convolve(fv_path_t * result, fv_path_t * stroke, fv_path_t * pen)
{
    fv_path_t hull;
    if(!fv_path_convex_hull(pen, &hull))
        return;

    int p = 0;
    for(int s = 0; s <= stroke->nsublen; s++) {
        int sublen = (s == stroke->nsublen) ? stroke->npoints : stroke->sublen[s];
        int npoints = sublen - p;
        if(npoints > 1) {
            fv_subpath_convolve(result, stroke->points + p, npoints, &hull);
            p = sublen;
        }
    }
    fv_path_cleanup(&hull);
}

/**********************
 *  SNAP / HINTING (for stroke fonts)
 **********************/

static fv_fixed_t fv_snap(fv_fixed_t v, const fv_fixed_t * snap, int n)
{
    for(int s = 0; s < n - 1; s++) {
        if(snap[s] <= v && v <= snap[s + 1]) {
            fv_fixed_t before = snap[s];
            fv_fixed_t after  = snap[s + 1];
            fv_fixed_t dist   = after - before;
            fv_fixed_t snap_before = SNAPI(before);
            fv_fixed_t snap_after  = SNAPI(after);
            fv_fixed_t move_before = snap_before - before;
            fv_fixed_t move_after  = snap_after - after;
            fv_fixed_t dist_before = v - before;
            fv_fixed_t dist_after  = after - v;
            fv_fixed_t move = (int32_t)((int64_t)dist_before * move_after +
                                        (int64_t)dist_after * move_before) / dist;
            v += move;
            break;
        }
    }
    return v;
}

static void fv_despeckle_small_stroke(fv_a8_target_t * t)
{
    if(t->width <= 0 || t->height <= 0) return;

    const int pixel_count = t->width * t->height;
    uint8_t * visited = lv_malloc_zeroed((size_t)pixel_count);
    int * queue = lv_malloc((size_t)pixel_count * sizeof(int));
    if(!visited || !queue) {
        lv_free(queue);
        lv_free(visited);
        return;
    }

    for(int y = 0; y < t->height; y++) {
        for(int x = 0; x < t->width; x++) {
            int idx = y * t->width + x;
            uint8_t alpha = t->buf[(size_t)y * t->stride + x];
            if(alpha < 24 || visited[idx]) continue;

            int head = 0;
            int tail = 0;
            int area = 0;
            uint16_t alpha_sum = 0;
            bool touches_strong = false;

            visited[idx] = 1;
            queue[tail++] = idx;

            while(head < tail) {
                int cur = queue[head++];
                int cy = cur / t->width;
                int cx = cur % t->width;
                uint8_t cur_alpha = t->buf[(size_t)cy * t->stride + cx];
                area++;
                alpha_sum = (uint16_t)(alpha_sum + cur_alpha);
                if(cur_alpha >= 112) touches_strong = true;

                for(int dy = -1; dy <= 1; dy++) {
                    for(int dx = -1; dx <= 1; dx++) {
                        if(dx == 0 && dy == 0) continue;
                        int nx = cx + dx;
                        int ny = cy + dy;
                        if(nx < 0 || nx >= t->width || ny < 0 || ny >= t->height) continue;
                        int nidx = ny * t->width + nx;
                        if(visited[nidx]) continue;
                        uint8_t nalpha = t->buf[(size_t)ny * t->stride + nx];
                        if(nalpha < 24) continue;
                        visited[nidx] = 1;
                        queue[tail++] = nidx;
                    }
                }
            }

            /* Cull tiny detached islands but keep real stems/joins. */
            if(area <= 2 && !touches_strong && alpha_sum <= 120) {
                for(int i = 0; i < tail; i++) {
                    int cur = queue[i];
                    int cy = cur / t->width;
                    int cx = cur % t->width;
                    t->buf[(size_t)cy * t->stride + cx] = 0;
                }
            }
        }
    }

    lv_free(queue);
    lv_free(visited);
}

/** Create a circular pen path for stroke rendering.
 *  Pen radius = scale / 24, minimum TWIN_FIXED_HALF.
 *  The pen is a unit circle scaled by the pen matrix. */
static void fv_create_pen(fv_path_t * pen, fv_fixed_t scale_x, fv_fixed_t scale_y)
{
    fv_fixed_t pen_x = SNAPH(scale_x / 24);
    fv_fixed_t pen_y = SNAPH(scale_y / 24);
    if(pen_x < FV_FIXED_HALF) pen_x = FV_FIXED_HALF;
    if(pen_y < FV_FIXED_HALF) pen_y = FV_FIXED_HALF;

    if(scale_x <= (16 * FV_FIXED_ONE)) {
        pen_x -= pen_x >> 3;
        pen_y -= pen_y >> 3;
    }

    /* Approximate circle with 16-point polygon for smoother small curves. */
    fv_path_init(pen);
    static const int8_t circle_x[] = { 16, 15, 11,  6,   0,  -6, -11, -15,
                                       -16, -15, -11, -6,  0,   6,  11,  15
                                     };
    static const int8_t circle_y[] = {  0,  6, 11, 15,  16,  15,  11,   6,
                                        0, -6, -11, -15, -16, -15, -11,  -6
                                     };
    for(int i = 0; i < 16; i++) {
        fv_sfixed_t cx = fv_fixed_to_sfixed((int32_t)circle_x[i] * pen_x / 16);
        fv_sfixed_t cy = fv_fixed_to_sfixed((int32_t)circle_y[i] * pen_y / 16);
        fv_path_sdraw(pen, cx, cy);
    }
}

/**********************
 *   RASTERIZER
 **********************/

static int fv_edge_compare_y(const void * a, const void * b)
{
    return (int)(((const fv_edge_t *)a)->top - ((const fv_edge_t *)b)->top);
}

/* Insertion sort for small arrays -- avoids qsort overhead for typical glyphs */
static void fv_edge_isort(fv_edge_t * edges, int n)
{
    for(int i = 1; i < n; i++) {
        fv_edge_t tmp = edges[i];
        int j = i - 1;
        while(j >= 0 && edges[j].top > tmp.top) {
            edges[j + 1] = edges[j];
            j--;
        }
        edges[j + 1] = tmp;
    }
}

static void fv_edge_step_by(fv_edge_t * edge, fv_sfixed_t dy)
{
    fv_dfixed_t e = edge->e + (fv_dfixed_t)dy * edge->dx;
    edge->x += edge->step_x * dy + edge->inc_x * (fv_sfixed_t)(e / edge->dy);
    edge->e = (fv_sfixed_t)(e % edge->dy);
}

static fv_sfixed_t fv_sfixed_grid_ceil(fv_sfixed_t f)
{
    return ((f + (FV_POLY_START - 1)) & ~(FV_POLY_STEP - 1)) + FV_POLY_START;
}

static int fv_edge_build(fv_spoint_t * vertices, int nvertices,
                         fv_edge_t * edges, fv_sfixed_t dx, fv_sfixed_t dy,
                         fv_sfixed_t top_y)
{
    int e = 0;
    for(int v = 0; v < nvertices; v++) {
        int nv = (v + 1 == nvertices) ? 0 : v + 1;
        int tv, bv;

        if(vertices[v].y == vertices[nv].y)
            continue;

        if(vertices[v].y < vertices[nv].y) {
            edges[e].winding = 1;
            tv = v;
            bv = nv;
        }
        else {
            edges[e].winding = -1;
            tv = nv;
            bv = v;
        }

        fv_sfixed_t y = fv_sfixed_grid_ceil(vertices[tv].y + dy);
        if(y < FV_POLY_START + top_y)
            y = FV_POLY_START + top_y;

        if(y >= vertices[bv].y + dy)
            continue;

        edges[e].dx = vertices[bv].x - vertices[tv].x;
        edges[e].dy = vertices[bv].y - vertices[tv].y;
        if(edges[e].dx >= 0)
            edges[e].inc_x = 1;
        else {
            edges[e].inc_x = -1;
            edges[e].dx = -edges[e].dx;
        }
        edges[e].step_x = edges[e].inc_x * (edges[e].dx / edges[e].dy);
        edges[e].dx = edges[e].dx % edges[e].dy;
        edges[e].top = vertices[tv].y + dy;
        edges[e].bot = vertices[bv].y + dy;
        edges[e].x   = vertices[tv].x + dx;
        edges[e].e    = 0;

        fv_edge_step_by(&edges[e], y - edges[e].top);
        edges[e].top = y;
        e++;
    }
    return e;
}

static void fv_span_fill(fv_a8_target_t * t, fv_sfixed_t y,
                         fv_sfixed_t left, fv_sfixed_t right)
{
    /* 4x4 coverage table */
    static const uint8_t coverage[4][4] = {
        {0x10, 0x10, 0x10, 0x10},
        {0x10, 0x10, 0x10, 0x10},
        {0x0f, 0x10, 0x10, 0x10},
        {0x10, 0x10, 0x10, 0x10},
    };

    const uint8_t * cover = coverage[(y >> FV_POLY_FIXED_SHIFT) & FV_POLY_MASK];
    int row = fv_sfixed_trunc(y);
    if(row < 0 || row >= t->height) return;
    uint8_t * span = t->buf + row * t->stride;

    /* Clip */
    if(left < fv_int_to_sfixed(0))
        left = fv_int_to_sfixed(0);
    if(right > fv_int_to_sfixed(t->width))
        right = fv_int_to_sfixed(t->width);

    /* Convert to sample grid */
    left  = fv_sfixed_grid_ceil(left) >> FV_POLY_FIXED_SHIFT;
    right = fv_sfixed_grid_ceil(right) >> FV_POLY_FIXED_SHIFT;

    if(right <= left) return;

    fv_sfixed_t x = left;
    uint8_t * s = span + (x >> FV_POLY_SHIFT);

    /* First partial pixel */
    if(x & FV_POLY_MASK) {
        uint16_t w = 0;
        int col = 0;
        while(x < right && (x & FV_POLY_MASK)) {
            w += cover[col++];
            x++;
        }
        uint16_t a = *s + w;
        *s++ = fv_sat(a);
    }

    /* Full coverage for middle pixels */
    uint16_t full_w = 0;
    for(int col = 0; col < FV_POLY_SAMPLE; col++)
        full_w += cover[col];

    while(x + FV_POLY_MASK < right) {
        uint16_t a = *s + full_w;
        *s++ = fv_sat(a);
        x += FV_POLY_SAMPLE;
    }

    /* Last partial pixel */
    if((right & FV_POLY_MASK) && x != right) {
        uint16_t w = 0;
        int col = 0;
        while(x < right) {
            w += cover[col++];
            x++;
        }
        uint16_t a = *s + w;
        *s = fv_sat(a);
    }
}

static void fv_edge_fill(fv_a8_target_t * target, fv_edge_t * edges, int nedges)
{
    fv_sfixed_t x0 = 0;

    if(nedges <= 0) return;

    /* Insertion sort for typical glyph edge counts (< 64); qsort for outliers */
    if(nedges <= 64)
        fv_edge_isort(edges, nedges);
    else
        qsort(edges, (size_t)nedges, sizeof(fv_edge_t), fv_edge_compare_y);

    int e = 0;
    fv_sfixed_t y = edges[0].top;
    fv_edge_t * active = NULL;

    for(;;) {
        /* Insert new edges */
        for(; e < nedges && edges[e].top <= y; e++) {
            fv_edge_t ** prev;
            fv_edge_t * a;
            for(prev = &active; (a = *prev); prev = &a->next)
                if(a->x > edges[e].x)
                    break;
            edges[e].next = *prev;
            *prev = &edges[e];
        }

        /* Walk active edges, fill spans */
        int w = 0;
        for(fv_edge_t * a = active; a; a = a->next) {
            if(w == 0)
                x0 = a->x;
            w += a->winding;
            if(w == 0)
                fv_span_fill(target, y, x0, a->x);
        }

        y += FV_POLY_STEP;
        if(fv_sfixed_trunc(y) >= target->height)
            break;

        /* Remove dead edges */
        {
            fv_edge_t ** prev = &active;
            fv_edge_t * a;
            while((a = *prev)) {
                if(a->bot <= y)
                    *prev = a->next;
                else
                    prev = &a->next;
            }
        }

        if(!active && e == nedges)
            break;

        /* Step all edges */
        for(fv_edge_t * a = active; a; a = a->next)
            fv_edge_step_by(a, FV_POLY_STEP);

        /* Re-sort by x (bubble pass) */
        {
            fv_edge_t ** prev = &active;
            fv_edge_t * a;
            fv_edge_t * n;
            while((a = *prev) && (n = a->next)) {
                if(a->x > n->x) {
                    a->next = n->next;
                    n->next = a;
                    *prev = n;
                    prev = &active;
                }
                else {
                    prev = &a->next;
                }
            }
        }
    }
}

static void fv_fill_path(fv_a8_target_t * target, fv_path_t * path,
                         fv_sfixed_t dx, fv_sfixed_t dy)
{
    int nalloc = path->npoints + path->nsublen + 1;
    fv_edge_t edge_stack[FV_EDGE_STACK];
    fv_edge_t * edges;
    bool heap_edges = false;

    if(nalloc <= FV_EDGE_STACK) {
        edges = edge_stack;
    }
    else {
        edges = lv_malloc((size_t)nalloc * sizeof(fv_edge_t));
        if(!edges) return;
        heap_edges = true;
    }

    int p = 0;
    int nedges = 0;
    for(int s = 0; s <= path->nsublen; s++) {
        int sublen = (s == path->nsublen) ? path->npoints : path->sublen[s];
        int npoints = sublen - p;
        if(npoints > 1) {
            int built = fv_edge_build(path->points + p, npoints,
                                      edges + nedges, dx, dy, 0);
            nedges += built;
            if(nedges > nalloc) {
                /* Safety: shouldn't happen but prevents buffer overrun */
                nedges = nalloc;
                break;
            }
            p = sublen;
        }
    }

    fv_edge_fill(target, edges, nedges);

    if(heap_edges) lv_free(edges);
}

/**********************
 *  OUTLINE TO PATH
 **********************/

/**
 * Parse outline commands into a path.
 * For stroke fonts: commands start after 6-byte header + snap data.
 * For TTF fonts: commands start after 1-byte advance.
 * @param snap_x  If non-NULL, apply snap hinting to coordinates (stroke fonts).
 * @param n_snap_x/y  Snap grid counts (0 if no snapping).
 */
static void fv_outline_to_path(fv_path_t * path, const int8_t * base, const int8_t * end,
                               fv_fixed_t scale_x, fv_fixed_t scale_y,
                               uint8_t font_type,
                               const fv_fixed_t * snap_x, int n_snap_x,
                               const fv_fixed_t * snap_y, int n_snap_y)
{
    const int8_t * g;
    if(font_type == LV_FONT_VEC_TYPE_STROKE) {
        /* Skip 6-byte header + snap_x + snap_y values */
        int skip = 6 + fv_glyph_n_snap_x(base) + fv_glyph_n_snap_y(base);
        g = base + skip;
    }
    else {
        g = base + 1; /* TTF: skip 1-byte advance */
    }
    fv_sfixed_t x1 = 0, y1 = 0;

    for(;;) {
        if(g >= end) break;
        int8_t op = *g++;
        switch(op) {
            case 'm': {
                    if(g + 2 > end) goto done;
                    fv_fixed_t fx = FV_SCALE_X(*g++, scale_x);
                    fv_fixed_t fy = FV_SCALE_Y(*g++, scale_y);
                    if(n_snap_x) fx = fv_snap(fx, snap_x, n_snap_x);
                    if(n_snap_y) fy = fv_snap(fy, snap_y, n_snap_y);
                    x1 = fv_fixed_to_sfixed(fx);
                    y1 = fv_fixed_to_sfixed(fy);
                    fv_path_smove(path, x1, y1);
                    break;
                }
            case 'l': {
                    if(g + 2 > end) goto done;
                    fv_fixed_t fx = FV_SCALE_X(*g++, scale_x);
                    fv_fixed_t fy = FV_SCALE_Y(*g++, scale_y);
                    if(n_snap_x) fx = fv_snap(fx, snap_x, n_snap_x);
                    if(n_snap_y) fy = fv_snap(fy, snap_y, n_snap_y);
                    x1 = fv_fixed_to_sfixed(fx);
                    y1 = fv_fixed_to_sfixed(fy);
                    fv_path_sdraw(path, x1, y1);
                    break;
                }
            case '2': {
                    if(g + 4 > end) goto done;
                    /* Quadratic Bezier: control + endpoint -> cubic */
                    fv_fixed_t cx_f = FV_SCALE_X(*g++, scale_x);
                    fv_fixed_t cy_f = FV_SCALE_Y(*g++, scale_y);
                    fv_fixed_t ex_f = FV_SCALE_X(*g++, scale_x);
                    fv_fixed_t ey_f = FV_SCALE_Y(*g++, scale_y);

                    fv_sfixed_t cx = fv_fixed_to_sfixed(cx_f);
                    fv_sfixed_t cy = fv_fixed_to_sfixed(cy_f);
                    fv_sfixed_t ex = fv_fixed_to_sfixed(ex_f);
                    fv_sfixed_t ey = fv_fixed_to_sfixed(ey_f);

                    /* Quadratic to cubic: CP1 = P0 + 2/3*(C-P0), CP2 = E + 2/3*(C-E) */
                    fv_sfixed_t c1x = x1 + 2 * (cx - x1) / 3;
                    fv_sfixed_t c1y = y1 + 2 * (cy - y1) / 3;
                    fv_sfixed_t c2x = ex + 2 * (cx - ex) / 3;
                    fv_sfixed_t c2y = ey + 2 * (cy - ey) / 3;

                    fv_path_scurve(path, c1x, c1y, c2x, c2y, ex, ey);
                    x1 = ex;
                    y1 = ey;
                    break;
                }
            case 'c': {
                    if(g + 6 > end) goto done;
                    /* Cubic Bezier: 3 control points */
                    fv_fixed_t ax_f = FV_SCALE_X(*g++, scale_x);
                    fv_fixed_t ay_f = FV_SCALE_Y(*g++, scale_y);
                    fv_fixed_t bx_f = FV_SCALE_X(*g++, scale_x);
                    fv_fixed_t by_f = FV_SCALE_Y(*g++, scale_y);
                    fv_fixed_t ex_f = FV_SCALE_X(*g++, scale_x);
                    fv_fixed_t ey_f = FV_SCALE_Y(*g++, scale_y);

                    if(n_snap_x) {
                        ax_f = fv_snap(ax_f, snap_x, n_snap_x);
                        bx_f = fv_snap(bx_f, snap_x, n_snap_x);
                        ex_f = fv_snap(ex_f, snap_x, n_snap_x);
                    }
                    if(n_snap_y) {
                        ay_f = fv_snap(ay_f, snap_y, n_snap_y);
                        by_f = fv_snap(by_f, snap_y, n_snap_y);
                        ey_f = fv_snap(ey_f, snap_y, n_snap_y);
                    }

                    fv_sfixed_t ax = fv_fixed_to_sfixed(ax_f);
                    fv_sfixed_t ay = fv_fixed_to_sfixed(ay_f);
                    fv_sfixed_t bx = fv_fixed_to_sfixed(bx_f);
                    fv_sfixed_t by = fv_fixed_to_sfixed(by_f);
                    x1 = fv_fixed_to_sfixed(ex_f);
                    y1 = fv_fixed_to_sfixed(ey_f);
                    fv_path_scurve(path, ax, ay, bx, by, x1, y1);
                    break;
                }
            case 'e':
                goto done;
            default:
                goto done;
        }
    }
done:
    fv_path_sfinish(path);
}

/**********************
 *  RASTERIZE GLYPH
 **********************/

static bool fv_rasterize_glyph(lv_font_vec_dsc_t * dsc, const int8_t * base,
                               uint16_t box_w, uint16_t box_h,
                               int16_t ofs_x, int16_t ofs_y,
                               lv_draw_buf_t * draw_buf)
{
    fv_fixed_t scale_x = (fv_fixed_t)dsc->pixel_size << 16;
    uint8_t font_type = dsc->data->type;
    /* TTF: negate Y to convert Y-up font coords to Y-down screen coords.
     * Stroke: Y is already negative-up (screen convention), no negation. */
    fv_fixed_t scale_y = (font_type == LV_FONT_VEC_TYPE_TTF) ? -scale_x : scale_x;
    const int8_t * outline_end = dsc->data->outlines + dsc->data->outlines_size;

    /* Compute snap grids for stroke fonts */
    fv_fixed_t snap_x_buf[FV_GLYPH_MAX_SNAP_X];
    fv_fixed_t snap_y_buf[FV_GLYPH_MAX_SNAP_Y];
    int n_snap_x = 0, n_snap_y = 0;
    if(font_type == LV_FONT_VEC_TYPE_STROKE) {
        n_snap_x = fv_glyph_n_snap_x(base);
        n_snap_y = fv_glyph_n_snap_y(base);
        if(n_snap_x > FV_GLYPH_MAX_SNAP_X) n_snap_x = FV_GLYPH_MAX_SNAP_X;
        if(n_snap_y > FV_GLYPH_MAX_SNAP_Y) n_snap_y = FV_GLYPH_MAX_SNAP_Y;
        const int8_t * sx = fv_glyph_snap_x(base);
        const int8_t * sy = fv_glyph_snap_y(base);
        for(int i = 0; i < n_snap_x; i++)
            snap_x_buf[i] = FV_SCALE_X(sx[i], scale_x);
        for(int i = 0; i < n_snap_y; i++)
            snap_y_buf[i] = FV_SCALE_Y(sy[i], scale_y);
    }

    /* Parse outline into stroke path */
    fv_path_t stroke;
    fv_path_init(&stroke);
    fv_outline_to_path(&stroke, base, outline_end, scale_x, scale_y,
                       font_type, snap_x_buf, n_snap_x, snap_y_buf, n_snap_y);

    if(stroke.npoints < 2) {
        fv_path_cleanup(&stroke);
        return false;
    }

    /* For stroke fonts: convolve with pen circle to produce filled polygon.
     * For TTF fonts: stroke IS already a filled polygon. */
    fv_path_t * fill_path;
    fv_path_t convolved;
    fv_path_t pen;

    if(font_type == LV_FONT_VEC_TYPE_STROKE) {
        fv_create_pen(&pen, scale_x, scale_y);
        fv_path_init(&convolved);
        fv_path_convolve(&convolved, &stroke, &pen);
        fv_path_cleanup(&pen);
        fill_path = &convolved;
    }
    else {
        fill_path = &stroke;
    }

    if(fill_path->npoints < 2) {
        if(font_type == LV_FONT_VEC_TYPE_STROKE) fv_path_cleanup(&convolved);
        fv_path_cleanup(&stroke);
        return false;
    }

    /* Compute path bounds */
    fv_sfixed_t min_x = FV_SFIXED_MAX, min_y = FV_SFIXED_MAX;
    fv_sfixed_t max_x = FV_SFIXED_MIN, max_y = FV_SFIXED_MIN;
    for(int i = 0; i < fill_path->npoints; i++) {
        fv_sfixed_t px = fill_path->points[i].x;
        fv_sfixed_t py = fill_path->points[i].y;
        if(px < min_x) min_x = px;
        if(px > max_x) max_x = px;
        if(py < min_y) min_y = py;
        if(py > max_y) max_y = py;
    }

    /* Set up render target */
    fv_a8_target_t target;
    target.buf    = draw_buf->data;
    target.width  = box_w;
    target.height = box_h;
    target.stride = (int32_t)draw_buf->header.stride;

    lv_memzero(target.buf, (size_t)target.stride * box_h);

    /* Translate path so min corner maps to pixel (0,0) */
    fv_sfixed_t dx = -min_x;
    fv_sfixed_t dy = -min_y;
    fv_fill_path(&target, fill_path, dx, dy);
    if(font_type == LV_FONT_VEC_TYPE_STROKE && dsc->pixel_size <= 16) {
        fv_despeckle_small_stroke(&target);
    }

    if(font_type == LV_FONT_VEC_TYPE_STROKE) fv_path_cleanup(&convolved);
    fv_path_cleanup(&stroke);
    return true;
}

/**********************
 *   L1 CACHE
 **********************/

#if LV_FONT_VEC_CACHE_L1_SETS > 0

/* FNV-1a over the (data, pixel_size, unicode) tuple.  Mixes the upper
 * pointer bits explicitly so 64-bit pointers don't collapse to a single
 * 32-bit hash zone -- otherwise distinct typefaces sharing the upper
 * bits of their data pointer would all hit the same set.
 *
 * FNV-1a propagates bits low->high; the set index uses
 * `& (SETS - 1)`, which sees only the low log2(SETS) bits.  Without a
 * finishing fold, characters that differ by exactly the set count
 * (e.g. 'A'=65 and 'a'=97 with 32 sets) and small pixel-size ints
 * (12/14/16/22/32) would cluster heavily.  An xor-shift fold spreads
 * the high bits into the low half before masking. */
static uint32_t fv_l1_hash(const void * data, int32_t pixel_size, uint32_t unicode)
{
    uintptr_t pi = (uintptr_t)data;
    uint32_t  h  = 2166136261u; /* FNV-1a offset basis */

    h = (h ^ (uint32_t)pi) * 16777619u;
#if UINTPTR_MAX > 0xFFFFFFFFu
    h = (h ^ (uint32_t)(pi >> 32)) * 16777619u;
#endif
    h = (h ^ (uint32_t)pixel_size) * 16777619u;
    h = (h ^ unicode) * 16777619u;

    /* xor-shift avalanche: fold high bits into the low half so the
     * `& (SETS - 1)` mask sees real entropy. */
    h ^= h >> 16;
    h *= 16777619u;
    h ^= h >> 13;
    return h;
}

static bool fv_l1_lookup(lv_font_vec_dsc_t * dsc, uint32_t unicode, lv_font_glyph_dsc_t * out)
{
    if(!s_l1_cache) return false;

    const void * data = dsc->data;
    int32_t      px   = dsc->pixel_size;
    uint32_t     idx  = fv_l1_hash(data, px, unicode) & (LV_FONT_VEC_CACHE_L1_SETS - 1);
    fv_l1_set_t * set = &s_l1_cache[idx];

    for(int w = 0; w < 2; w++) {
        const fv_l1_entry_t * e = &set->ways[w];
        if(e->unicode == unicode && e->pixel_size == px && e->data == data) {
            set->lru_bits = (uint8_t)w;
            *out = e->dsc;
            s_l1_hits++;
            return true;
        }
    }
    s_l1_misses++;
    return false;
}

static void fv_l1_fill(lv_font_vec_dsc_t * dsc, uint32_t unicode, const lv_font_glyph_dsc_t * d)
{
    if(!s_l1_cache) return;

    const void * data = dsc->data;
    int32_t      px   = dsc->pixel_size;
    uint32_t     idx  = fv_l1_hash(data, px, unicode) & (LV_FONT_VEC_CACHE_L1_SETS - 1);
    fv_l1_set_t * set = &s_l1_cache[idx];

    uint32_t victim = (set->lru_bits == 0) ? 1u : 0u;
    fv_l1_entry_t * e = &set->ways[victim];
    e->data       = data;
    e->pixel_size = px;
    e->unicode    = unicode;
    e->dsc        = *d;
    e->dsc.entry  = NULL; /* L1 entries are not cache-ref'd */
    set->lru_bits = (uint8_t)victim;
}

/* Refcounted lifecycle: first init allocates the shared table, last
 * deinit frees it.  Allocation failure is non-fatal -- lookup/fill
 * gracefully no-op when the table is NULL, falling back to the L2
 * bitmap cache and outline walks. */
static void fv_l1_acquire(void)
{
    if(s_l1_refs == 0) {
        LV_ASSERT(s_l1_cache == NULL);
        s_l1_cache = lv_malloc_zeroed(sizeof(fv_l1_set_t) * LV_FONT_VEC_CACHE_L1_SETS);
        LV_ASSERT_MALLOC(s_l1_cache);
    }
    s_l1_refs++;
}

static void fv_l1_release(void)
{
    LV_ASSERT(s_l1_refs > 0);
    if(--s_l1_refs == 0) {
        /* s_l1_cache may legitimately be NULL if the initial lv_malloc
         * failed; lv_free(NULL) is a no-op so no extra guard needed. */
        lv_free(s_l1_cache);
        s_l1_cache = NULL;
    }
}

/* Invalidate every entry keyed on (data, pixel_size).  Sets `data = NULL`
 * so the lookup's `e->data == data` check fails for any non-NULL key.
 * Cheap (32 sets * 2 ways = 64 comparisons), called only on font teardown. */
static void fv_l1_invalidate(const void * data, int32_t pixel_size)
{
    if(!s_l1_cache) return;
    for(uint32_t i = 0; i < LV_FONT_VEC_CACHE_L1_SETS; i++) {
        for(int w = 0; w < 2; w++) {
            fv_l1_entry_t * e = &s_l1_cache[i].ways[w];
            if(e->data == data && e->pixel_size == pixel_size) {
                e->data = NULL;
            }
        }
    }
}

#endif /* LV_FONT_VEC_CACHE_L1_SETS > 0 */

/**********************
 *   L2 CACHE
 **********************/

#if LV_FONT_VEC_CACHE_SIZE > 0

static bool fv_bitmap_create_cb(lv_font_vec_bitmap_node_t * node, void * user_data)
{
    LV_PROFILER_FONT_BEGIN;
    lv_font_t * font = (lv_font_t *)user_data;
    lv_font_vec_dsc_t * dsc = (lv_font_vec_dsc_t *)font->dsc;
    /* Side-channel back to vec_get_glyph_bitmap_cb so it can attribute
     * the just-completed acquire_or_create call as a miss. */
    s_l2_create_invoked = true;

    /* Metrics are pre-computed by get_glyph_dsc and passed via the search key.
     * The cache framework copies the key into the node before calling create_cb,
     * so node->box_w/box_h/ofs_x/ofs_y are already set. No need to re-walk the outline. */
    uint16_t box_w = node->box_w;
    uint16_t box_h = node->box_h;

    if(box_w == 0 || box_h == 0) {
        node->draw_buf = NULL;
        node->slot.size = sizeof(lv_font_vec_bitmap_node_t);
        LV_PROFILER_FONT_END;
        return true;
    }

    const int8_t * base = fv_glyph_base(dsc, node->unicode);
    if(!base) {
        LV_PROFILER_FONT_END;
        return false;
    }

    uint32_t stride = lv_draw_buf_width_to_stride(box_w, LV_COLOR_FORMAT_A8);
    node->draw_buf = lv_draw_buf_create_ex(font_draw_buf_handlers,
                                           box_w, box_h, LV_COLOR_FORMAT_A8, stride);
    if(!node->draw_buf) {
        LV_LOG_WARN("vec font: draw_buf alloc failed for U+%04X", node->unicode);
        LV_PROFILER_FONT_END;
        return false;
    }

    fv_rasterize_glyph(dsc, base, box_w, box_h, node->ofs_x, node->ofs_y, node->draw_buf);

    lv_draw_buf_flush_cache(node->draw_buf, NULL);

    node->slot.size = sizeof(lv_font_vec_bitmap_node_t) + stride * box_h;

    LV_PROFILER_FONT_END;
    return true;
}

static void fv_bitmap_free_cb(lv_font_vec_bitmap_node_t * node, void * user_data)
{
    LV_UNUSED(user_data);
    if(node->draw_buf) {
        lv_draw_buf_destroy(node->draw_buf);
        node->draw_buf = NULL;
    }
}

static lv_cache_compare_res_t fv_bitmap_compare_cb(const lv_font_vec_bitmap_node_t * a,
                                                   const lv_font_vec_bitmap_node_t * b)
{
    if(a->unicode != b->unicode)
        return a->unicode > b->unicode ? 1 : -1;
    return 0;
}

#endif /* LV_FONT_VEC_CACHE_SIZE > 0 */

/**********************
 *  FONT CALLBACKS
 **********************/

static bool vec_get_glyph_dsc_cb(const lv_font_t * font, lv_font_glyph_dsc_t * dsc_out,
                                 uint32_t letter, uint32_t letter_next)
{
    LV_UNUSED(letter_next);
    LV_PROFILER_FONT_BEGIN;

    if(letter < 0x20) {
        dsc_out->adv_w  = 0;
        dsc_out->box_w  = 0;
        dsc_out->box_h  = 0;
        dsc_out->ofs_x  = 0;
        dsc_out->ofs_y  = 0;
        dsc_out->format = LV_FONT_GLYPH_FORMAT_NONE;
        LV_PROFILER_FONT_END;
        return true;
    }

    lv_font_vec_dsc_t * dsc = (lv_font_vec_dsc_t *)font->dsc;

#if LV_USE_LOG
    s_vec_dsc_calls++;
#endif

    /* L1 lookup */
#if LV_FONT_VEC_CACHE_L1_SETS > 0
    if(fv_l1_lookup(dsc, letter, dsc_out)) {
        LV_PROFILER_FONT_END;
        return true;
    }
#endif

    const int8_t * base = fv_glyph_base(dsc, letter);
    if(!base) {
        dsc_out->is_placeholder = 1;
        LV_PROFILER_FONT_END;
        return true;
    }

    fv_fixed_t scale = (fv_fixed_t)dsc->pixel_size << 16;

    int32_t box_l, box_t, box_r, box_b;

    if(dsc->data->type == LV_FONT_VEC_TYPE_STROKE) {
        /* Stroke font: metrics from glyph header + pen expansion.
         *
         * Mado shifts the rendering origin right by (margin + pen) = 2*pen
         * so the pen expansion never goes negative.  We match that here:
         *
         *   left_bearing  = glyph_left  + margin    (shifted right)
         *   right_bearing = glyph_right + 2*pen + margin
         *   advance       = right_bearing + margin  (= glyph_right + 4*pen)
         *
         * The bbox accounts for pen expansion on all sides.
         */
        fv_fixed_t pen_w = SNAPH(scale / 24);
        if(pen_w < FV_FIXED_HALF) pen_w = FV_FIXED_HALF;
        fv_fixed_t margin = pen_w;

        /* Advance width: glyph_right + pen expansion.
         * The pen expands the rightmost stroke by pen_radius.
         * Add one pen_radius as right side bearing. */
        fv_fixed_t g_right_scaled = FV_SCALE_X(fv_glyph_right(base), scale);
        fv_fixed_t adv = g_right_scaled + pen_w * 2 + margin;
        dsc_out->adv_w = (uint16_t)((adv + FV_FIXED_HALF) >> 16);

        /* Bbox: the rasterizer computes the actual convolved path bounds and
         * translates to (0,0).  We set ofs_x/ofs_y = 0 so LVGL draws the
         * bitmap at the cursor origin, and include the full pen expansion
         * in the advance width instead. This avoids negative ofs_x which
         * causes first-character clipping in centered/constrained labels. */
        fv_fixed_t pad   = pen_w + FV_FIXED_ONE;
        fv_fixed_t right = g_right_scaled + pen_w * 2 + pad;
        /* ascent is positive in header (units above baseline = negative screen Y) */
        fv_fixed_t g_top = FV_SCALE(-(fv_glyph_ascent(base)), scale) - pad;
        /* descent is positive in header (units below baseline = positive screen Y) */
        fv_fixed_t g_bot = FV_SCALE(fv_glyph_descent(base), scale) + pad;

        box_l = 0;
        box_t = (int32_t)g_top >> 16;
        box_r = (int32_t)((right + pad + 0xffff) >> 16);
        box_b = (int32_t)((g_bot + 0xffff) >> 16);
    }
    else {
        /* TTF font: advance from outline[0], bbox from outline walk */
        int32_t advance_gfixed = base[0];
        dsc_out->adv_w = (uint16_t)((advance_gfixed * dsc->pixel_size + (FV_GFIXED_ONE / 2)) / FV_GFIXED_ONE);

        fv_fixed_t left, top, right, bottom;
        const int8_t * outline_end = dsc->data->outlines + dsc->data->outlines_size;
        fv_outline_bbox(base, outline_end, scale, -scale, &left, &top, &right, &bottom);

        box_l = (int32_t)left >> 16;
        box_t = (int32_t)top >> 16;
        box_r = (int32_t)((right + 0xffff) >> 16);
        box_b = (int32_t)((bottom + 0xffff) >> 16);
    }

    dsc_out->box_w  = (uint16_t)(box_r - box_l);
    dsc_out->box_h  = (uint16_t)(box_b - box_t);
    dsc_out->ofs_x  = (int16_t)box_l;

    dsc_out->ofs_y  = (int16_t)(-(box_b)); /* LVGL: distance from baseline to bottom of box, positive = above */
    dsc_out->format = LV_FONT_GLYPH_FORMAT_A8;
    dsc_out->stride = (uint16_t)lv_draw_buf_width_to_stride(dsc_out->box_w, LV_COLOR_FORMAT_A8);
    dsc_out->is_placeholder = 0;
    dsc_out->gid.index = letter; /* store unicode for bitmap cache key */

#if LV_FONT_VEC_CACHE_L1_SETS > 0
    fv_l1_fill(dsc, letter, dsc_out);
#endif

    LV_PROFILER_FONT_END;
    return true;
}

static const void * vec_get_glyph_bitmap_cb(lv_font_glyph_dsc_t * g_dsc, lv_draw_buf_t * draw_buf)
{
    LV_PROFILER_FONT_BEGIN;
    const lv_font_t * font = g_dsc->resolved_font;
    lv_font_vec_dsc_t * dsc = (lv_font_vec_dsc_t *)font->dsc;

#if LV_USE_LOG
    s_vec_bitmap_calls++;
#endif
    uint32_t unicode = g_dsc->gid.index; /* stored by get_glyph_dsc */

#if LV_FONT_VEC_CACHE_SIZE > 0
    if(dsc->bitmap_cache && lv_cache_is_enabled(dsc->bitmap_cache)) {
        uint32_t stride = lv_draw_buf_width_to_stride(g_dsc->box_w, LV_COLOR_FORMAT_A8);
        lv_font_vec_bitmap_node_t search_key = {
            .slot.size = sizeof(lv_font_vec_bitmap_node_t) + stride * g_dsc->box_h,
            .unicode = unicode,
            .box_w = g_dsc->box_w,
            .box_h = g_dsc->box_h,
            .ofs_x = g_dsc->ofs_x,
            .ofs_y = g_dsc->ofs_y,
        };

        s_l2_create_invoked = false;
        lv_cache_entry_t * entry = lv_cache_acquire_or_create(dsc->bitmap_cache,
                                                              &search_key,
                                                              (void *)font);
        if(entry) {
            if(s_l2_create_invoked) s_l2_misses++;
            else                    s_l2_hits++;
            g_dsc->entry = entry;
            lv_font_vec_bitmap_node_t * node = lv_cache_entry_get_data(entry);
            LV_PROFILER_FONT_END;
            return node->draw_buf;
        }
    }
#endif

    /* Fallback: rasterize directly into caller's draw_buf (cache disabled or miss).
     * Mirror the cache-hit path's flush -- without it, GPU/DMA backends that
     * read the bitmap from SRAM see stale data after a CPU-side rasterization. */
    if(draw_buf && g_dsc->box_w > 0 && g_dsc->box_h > 0) {
        const int8_t * base = fv_glyph_base(dsc, unicode);
        if(base) {
            fv_rasterize_glyph(dsc, base, g_dsc->box_w, g_dsc->box_h,
                               g_dsc->ofs_x, g_dsc->ofs_y, draw_buf);
            lv_draw_buf_flush_cache(draw_buf, NULL);
            LV_PROFILER_FONT_END;
            return draw_buf;
        }
    }

    LV_PROFILER_FONT_END;
    return NULL;
}

static void vec_release_glyph_cb(const lv_font_t * font, lv_font_glyph_dsc_t * g_dsc)
{
    LV_ASSERT_NULL(font);
    if(!g_dsc->entry) return;

#if LV_FONT_VEC_CACHE_SIZE > 0
    lv_font_vec_dsc_t * dsc = (lv_font_vec_dsc_t *)font->dsc;
    if(dsc->bitmap_cache) {
        lv_cache_release(dsc->bitmap_cache, g_dsc->entry, NULL);
    }
#endif
    g_dsc->entry = NULL;
}

#endif /* LV_USE_FONT_VEC */
