/**
 * @file mem_report.c
 *
 * Steady-state heap breakdown for the cellphone demo. Walks the active
 * screen tree and reports per-class object/style accounting against the
 * live LVGL pool.
 *
 * Opt-in diagnostic: the entire body is gated on CELLPHONE_TEST_REPORT
 * (the same flag main_test.c uses to call cellphone_mem_report). When
 * the flag is unset -- which is the case for the default lvgl_demos
 * library build -- this translation unit collapses to nothing, keeping
 * printf/qsort and the private-header coupling out of every cellphone
 * binary that doesn't explicitly ask for it. Compile this file directly
 * into the test executable with -DCELLPHONE_TEST_REPORT instead of
 * relying on the demos library to carry it.
 *
 * Couples to LVGL internals (lv_obj_private.h, lv_tlsf_private.h,
 * lv_global_t.tlsf_state) and to the builtin TLSF allocator. If
 * LVGL refactors any of those, this file breaks at compile time --
 * acceptable trade for a diagnostic that prints exact pool occupancy.
 */

#include "lvgl.h"
#include "mem_report.h"
#include "lv_demo_cellphone_common.h"

#if defined(CELLPHONE_TEST_REPORT)

#if LV_USE_STDLIB_MALLOC != LV_STDLIB_BUILTIN
    #error "mem_report.c requires the builtin TLSF allocator (LV_USE_STDLIB_MALLOC == LV_STDLIB_BUILTIN)"
#endif

#include "../../src/core/lv_obj_private.h"
#include "../../src/core/lv_obj_class_private.h"
#include "../../src/core/lv_obj_style_private.h"
#include "../../src/core/lv_global.h"
#include "../../src/stdlib/builtin/lv_tlsf.h"
#include "../../src/stdlib/builtin/lv_tlsf_private.h"
#include "../../include/lvgl/misc/lv_ll.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define MAX_CLASSES 64

typedef struct {
    const lv_obj_class_t * cls;
    const char      *      name;
    uint32_t               count;
    uint32_t               instance_bytes; /* sum of class->instance_size */
    uint32_t               styles_total;   /* sum of obj->style_cnt across instances */
    uint32_t               with_spec;      /* count having spec_attr allocated */
} class_bucket_t;

static class_bucket_t s_buckets[MAX_CLASSES];
static int            s_bucket_count;

static class_bucket_t * bucket_get(const lv_obj_class_t * cls)
{
    for(int i = 0; i < s_bucket_count; i++) {
        if(s_buckets[i].cls == cls) return &s_buckets[i];
    }
    if(s_bucket_count >= MAX_CLASSES) return &s_buckets[0];
    class_bucket_t * b = &s_buckets[s_bucket_count++];
    b->cls            = cls;
    b->name           = cls->name ? cls->name : "(unnamed)";
    b->count          = 0;
    b->instance_bytes = 0;
    b->styles_total   = 0;
    b->with_spec      = 0;
    return b;
}

static void walk(lv_obj_t * obj)
{
    if(!obj) return;
    class_bucket_t * b = bucket_get(obj->class_p);
    b->count++;
    b->instance_bytes += obj->class_p->instance_size;
    b->styles_total   += obj->style_cnt;
    if(obj->spec_attr) b->with_spec++;
    uint32_t cc = lv_obj_get_child_count(obj);
    for(uint32_t i = 0; i < cc; i++) {
        walk(lv_obj_get_child(obj, i));
    }
}

static int cmp_bytes_desc(const void * a, const void * b)
{
    const class_bucket_t * x = (const class_bucket_t *)a;
    const class_bucket_t * y = (const class_bucket_t *)b;
    const uint32_t style_sz = (uint32_t)sizeof(lv_obj_style_t);
    const uint32_t spec_sz  = (uint32_t)sizeof(lv_obj_spec_attr_t);
    uint32_t bx = x->instance_bytes + x->styles_total * style_sz + x->with_spec * spec_sz;
    uint32_t by = y->instance_bytes + y->styles_total * style_sz + y->with_spec * spec_sz;
    if(bx > by) return -1;
    if(bx < by) return 1;
    return 0;
}

/* ----- TLSF live-block histogram ----- */

#define HIST_BUCKETS 12
typedef struct {
    size_t lo;     /* inclusive */
    size_t hi;     /* exclusive */
    uint32_t cnt;
    size_t bytes;
} hist_bucket_t;

static hist_bucket_t s_hist[HIST_BUCKETS] = {
    { 0,    16,    0, 0 },
    { 16,   32,    0, 0 },
    { 32,   64,    0, 0 },
    { 64,   128,   0, 0 },
    { 128,  256,   0, 0 },
    { 256,  512,   0, 0 },
    { 512,  1024,  0, 0 },
    { 1024, 2048,  0, 0 },
    { 2048, 4096,  0, 0 },
    { 4096, 8192,  0, 0 },
    { 8192, 16384, 0, 0 },
    { 16384, (size_t) -1, 0, 0 },
};

static uint32_t s_total_blocks;
static size_t   s_total_bytes;

/* Track largest blocks for inspection. */
#define TOPN 12
static struct {
    void * ptr;
    size_t size;
} s_top[TOPN];

static void hist_walker(void * ptr, size_t size, int used, void * user)
{
    LV_UNUSED(user);
    if(!used) return;
    s_total_blocks++;
    s_total_bytes += size;
    for(int i = 0; i < HIST_BUCKETS; i++) {
        if(size >= s_hist[i].lo && size < s_hist[i].hi) {
            s_hist[i].cnt++;
            s_hist[i].bytes += size;
            break;
        }
    }
    /* Update top-N (insertion sort, descending by size). */
    for(int i = 0; i < TOPN; i++) {
        if(size > s_top[i].size) {
            for(int j = TOPN - 1; j > i; j--) s_top[j] = s_top[j - 1];
            s_top[i].ptr = ptr;
            s_top[i].size = size;
            break;
        }
    }
}

static void mem_report_walk_pool(void)
{
    lv_tlsf_state_t * st = &LV_GLOBAL_DEFAULT()->tlsf_state;
    lv_pool_t * pool_p;
    LV_LL_READ(&st->pool_ll, pool_p) {
        lv_tlsf_walk_pool(*pool_p, hist_walker, NULL);
    }
}

#if LV_USE_FONT_VEC
typedef struct {
    const char * name;
    const lv_font_t * font;
} vec_font_slot_t;

static bool vec_font_seen(const lv_font_t * const * seen_fonts, uint32_t seen_count,
                          const lv_font_t * font)
{
    for(uint32_t i = 0; i < seen_count; i++) {
        if(seen_fonts[i] == font) return true;
    }
    return false;
}

static void report_vec_font_chain(const char * chain_name, const lv_font_t * font,
                                  const lv_font_t ** seen_fonts, uint32_t seen_cap,
                                  uint32_t * seen_count,
                                  size_t * total_used, size_t * total_max)
{
    uint32_t depth = 0;

    if(font == NULL) {
        printf("  %-12s : not configured\n", chain_name);
        return;
    }

    while(font != NULL) {
        char slot_name[32];
        if(depth == 0) {
            lv_snprintf(slot_name, sizeof(slot_name), "%s", chain_name);
        }
        else {
            lv_snprintf(slot_name, sizeof(slot_name), "%s->fb%" LV_PRIu32,
                        chain_name, depth);
        }

        if(vec_font_seen(seen_fonts, *seen_count, font)) {
            printf("  %-12s : shared with earlier chain\n", slot_name);
            return;
        }

        if(*seen_count >= seen_cap) {
            printf("  %-12s : seen-buffer full (cap=%" LV_PRIu32 "), truncating\n",
                   slot_name, seen_cap);
            return;
        }

        seen_fonts[*seen_count] = font;
        (*seen_count)++;

        int32_t px = lv_font_vec_get_instance_pixel_size(font);
        if(px <= 0) {
            printf("  %-12s : non-vec font\n", slot_name);
            font = font->fallback;
            depth++;
            continue;
        }

        size_t used = 0;
        size_t max = 0;
        if(!lv_font_vec_get_instance_l2_size(font, &used, &max)) {
            printf("  %-12s : pixel=%" LV_PRId32 ", L2 disabled\n", slot_name, px);
            font = font->fallback;
            depth++;
            continue;
        }

        *total_used += used;
        *total_max += max;
        printf("  %-12s : pixel=%" LV_PRId32 ", L2=%zu/%zu B\n",
               slot_name, px, used, max);

        font = font->fallback;
        depth++;
    }
}

static size_t report_vec_font_caches(void)
{
#if LV_FONT_VEC_CACHE_SIZE > 0
    const cellphone_theme_t * theme = cellphone_theme_active();
    size_t total_used = 0;
    size_t total_max = 0;
    const lv_font_t * seen_fonts[16];
    uint32_t seen_count = 0;
    uint32_t l1_hits = 0, l1_misses = 0;
    uint32_t l2_hits = 0, l2_misses = 0;
    const vec_font_slot_t slots[] = {
        { "sm(12)", theme->font_sm },
        { "normal(14)", theme->font_normal },
        { "heading(16)", theme->font_heading },
        { "large(22)", theme->font_large },
        { "clock(32)", theme->font_clock },
    };

    printf("\nVector font caches:\n");
    for(uint32_t i = 0; i < sizeof(slots) / sizeof(slots[0]); i++) {
        report_vec_font_chain(slots[i].name, slots[i].font,
                              seen_fonts, sizeof(seen_fonts) / sizeof(seen_fonts[0]),
                              &seen_count, &total_used, &total_max);
    }

    lv_font_vec_get_l1_stats(&l1_hits, &l1_misses);
    lv_font_vec_get_l2_stats(&l2_hits, &l2_misses);
    printf("  total L2       : %zu/%zu B\n", total_used, total_max);
    printf("  lifetime L1    : hits=%" LV_PRIu32 " misses=%" LV_PRIu32 "\n",
           l1_hits, l1_misses);
    printf("  lifetime L2    : hits=%" LV_PRIu32 " misses=%" LV_PRIu32 "\n",
           l2_hits, l2_misses);
    return total_used;
#else
    printf("\nVector font caches:\n");
    printf("  disabled at build time (LV_FONT_VEC_CACHE_SIZE == 0)\n");
    return 0;
#endif
}
#endif

void cellphone_mem_report(void)
{
    s_bucket_count = 0;
    memset(s_top, 0, sizeof(s_top));
    for(int i = 0; i < HIST_BUCKETS; i++) {
        s_hist[i].cnt = 0;
        s_hist[i].bytes = 0;
    }
    s_total_blocks = 0;
    s_total_bytes = 0;

    lv_display_t * disp = lv_display_get_default();
    if(!disp) {
        printf("[mem_report] no default display\n");
        return;
    }

    uint32_t total_objs = 0;
    uint32_t total_styles = 0;
    uint32_t total_spec = 0;
    uint32_t total_instance = 0;

    printf("\n=== Heap breakdown (steady-state) ===\n");

    /* Walk the active screen and the three system layers. Inactive screens
     * pushed earlier and not yet deleted would not be reflected here, but
     * cellphone_screen_pop() schedules a delayed delete and Test 11 waits
     * past it, so the only live tree is the home screen + layers. */
    lv_obj_t * scr_act = lv_display_get_screen_active(disp);
    lv_obj_t * scr_top = lv_display_get_layer_top(disp);
    lv_obj_t * scr_sys = lv_display_get_layer_sys(disp);
    lv_obj_t * scr_bot = lv_display_get_layer_bottom(disp);

    walk(scr_act);
    walk(scr_top);
    walk(scr_sys);
    walk(scr_bot);

    for(int i = 0; i < s_bucket_count; i++) {
        total_objs     += s_buckets[i].count;
        total_styles   += s_buckets[i].styles_total;
        total_spec     += s_buckets[i].with_spec;
        total_instance += s_buckets[i].instance_bytes;
    }

    qsort(s_buckets, s_bucket_count, sizeof(s_buckets[0]), cmp_bytes_desc);

    printf("Per-class accounting (sorted by approx bytes):\n");
    printf("  %-22s %5s %8s %8s %8s\n",
           "class", "n", "instB", "styles", "specs");
    for(int i = 0; i < s_bucket_count; i++) {
        printf("  %-22s %5u %8u %8u %8u\n",
               s_buckets[i].name,
               (unsigned)s_buckets[i].count,
               (unsigned)s_buckets[i].instance_bytes,
               (unsigned)s_buckets[i].styles_total,
               (unsigned)s_buckets[i].with_spec);
    }

    /* Use sizeof at runtime so the report tracks the build's ABI
     * (32-bit MCU shrinks both structs vs the 64-bit host). */
    const uint32_t style_entry_bytes = (uint32_t)sizeof(lv_obj_style_t);
    const uint32_t spec_attr_bytes   = (uint32_t)sizeof(lv_obj_spec_attr_t);
    uint32_t obj_bytes   = total_instance;
    uint32_t style_bytes = total_styles * style_entry_bytes;
    uint32_t spec_bytes  = total_spec * spec_attr_bytes;

    lv_mem_monitor_t m;
    lv_mem_monitor(&m);
    size_t pool_used = (size_t)(m.total_size - m.free_size);

    printf("\nTotals:\n");
    printf("  objects               : %u  (sum sizeof = %u bytes)\n",
           (unsigned)total_objs, (unsigned)obj_bytes);
    printf("  style[] entries       : %u  (~%u bytes @%u/entry)\n",
           (unsigned)total_styles, (unsigned)style_bytes,
           (unsigned)style_entry_bytes);
    printf("  spec_attr blocks      : %u  (~%u bytes @%u/entry)\n",
           (unsigned)total_spec, (unsigned)spec_bytes,
           (unsigned)spec_attr_bytes);
    printf("  ---\n");
    uint32_t accounted = obj_bytes + style_bytes + spec_bytes;
    printf("  accounted by tree     : ~%u bytes (%u KiB)\n",
           (unsigned)accounted, (unsigned)(accounted / 1024));
    printf("  pool used (monitor)   : %zu bytes (%zu KiB)\n",
           pool_used, pool_used / 1024);
#if LV_USE_FONT_VEC
    /* Print the vec-font cache section unconditionally so its presence
     * is not coupled to whether the residual accounting is non-zero. */
    size_t vec_cache_used = report_vec_font_caches();
#endif
    if(pool_used > accounted) {
        size_t residual_sz = pool_used - accounted;
        uint32_t residual = (uint32_t)residual_sz;
#if LV_USE_FONT_VEC
        size_t vec_l2 = vec_cache_used < residual_sz ? vec_cache_used : residual_sz;
        size_t residual_other = residual_sz - vec_l2;
        printf("  residual breakdown    : vec L2 ~%zu B, other ~%zu B\n",
               vec_l2, residual_other);
#endif
        printf("  residual (font cache, theme styles, animation,\n");
        printf("            event lists, child arrays, ...): ~%u bytes (%u KiB)\n",
               (unsigned)residual, (unsigned)(residual / 1024));
    }

    /* TLSF live-block histogram */
    mem_report_walk_pool();
    printf("\nLive allocation histogram (%u blocks, %zu bytes total):\n",
           (unsigned)s_total_blocks, s_total_bytes);
    printf("  %-12s %6s %10s\n", "size range", "count", "bytes");
    for(int i = 0; i < HIST_BUCKETS; i++) {
        if(!s_hist[i].cnt) continue;
        char range[32];
        if(s_hist[i].hi == (size_t) -1)
            snprintf(range, sizeof(range), ">=%zu", s_hist[i].lo);
        else
            snprintf(range, sizeof(range), "%zu..%zu", s_hist[i].lo, s_hist[i].hi - 1);
        printf("  %-12s %6u %10zu\n", range, (unsigned)s_hist[i].cnt, s_hist[i].bytes);
    }
    printf("\nTop %d largest live blocks:\n", TOPN);
    for(int i = 0; i < TOPN; i++) {
        if(!s_top[i].size) break;
        printf("  #%-2d  size=%-8zu  ptr=%p\n", i + 1, s_top[i].size, s_top[i].ptr);
    }
}

#endif /* CELLPHONE_TEST_REPORT */
