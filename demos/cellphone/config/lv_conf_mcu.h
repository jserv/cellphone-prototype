/**
 * @file lv_conf_mcu.h
 *
 * Shipping config for the cellphone demo. Sizes the LVGL pool only; the
 * surrounding SRAM budget (display draw buffer, libc heap, task stacks,
 * app globals) is the integrator's responsibility and is NOT included
 * here. The numbers below assume QVGA 240x320, RGB565, partial-render
 * draw buffers, no external PSRAM/SDRAM. Keeps the simulator config
 * (`lv_conf.h`) intact for development; opt in for a target build with:
 *
 *   cmake -DLV_BUILD_CONF_PATH=demos/cellphone/config/lv_conf_mcu.h ...
 *
 * Differences from the simulator config:
 *
 *   LV_MEM_SIZE     1 MiB   -> 76 KiB  (peak pool ~71 KiB on macOS,
 *                              ~72 KiB on node1/Linux from allocator
 *                              high-water mark during `cellphone_test`
 *                              Test 11. The cumulative effect of
 *                              shared L1 metrics, mixed-budget L2,
 *                              cache disable for heading/large/clock,
 *                              and 2 KiB caps on sm/normal cut
 *                              end-of-cycle from 61 -> 40 KiB and peak
 *                              from 90 -> 71 KiB on macOS. Pool size
 *                              76 KiB leaves ~4-5 KiB headroom on the
 *                              measured workloads. Tighten only after a
 *                              fresh Test 11 gate;
 *                              the workload's worst-case is the SMS
 *                              chat-detail render across the 5-cycle
 *                              push/pop batch.)
 *   LV_USE_SDL      1       -> 0       (no SDL on target)
 *   LV_USE_LOG      1       -> 0       (printf-only logger, off in shipping)
 *   LV_USE_SNAPSHOT 1       -> 0       (test-harness only)
 *   LV_GRADIENT_MAX_STOPS 6 -> 2       (deferred E10 not in shipping path)
 *   LV_DEMO_CELLPHONE_SKIN  -> 0       (SDL-only feature)
 *
 * Indicative SRAM budget. Two tiers, picked by the integrator's draw-buffer
 * strategy. With the 76 KiB pool both tiers fit comfortably; pick the
 * 128 KiB tier when partial-scroll latency or flush-callback overhead
 * matters and the extra 16 KiB SRAM is available.
 *
 * 112 KiB-SRAM tier (single partial draw buffer, more flush callbacks):
 *
 *   LVGL pool (this file)                      76 KiB
 *   One 15-line RGB565 partial draw buffer     ~7 KiB  (15*240*2 B)
 *   Task stacks + libc heap + globals          ~20 KiB (rule of thumb)
 *   ----------------------------------------------------
 *   Total                                     ~103 KiB / 112 KiB
 *
 * 128 KiB-SRAM tier (double-buffered partial draw, fewer flushes):
 *
 *   LVGL pool (this file)                      76 KiB
 *   Two 30-line RGB565 partial draw buffers    ~28 KiB (30*240*2*2 B)
 *   Task stacks + libc heap + globals          ~20 KiB (rule of thumb)
 *   ----------------------------------------------------
 *   Total                                     ~124 KiB / 128 KiB
 *
 * Single-buffer can force the renderer onto a slow path for partial
 * scroll regions and increases flush-callback frequency on the display
 * link. Profile both paths on the target panel before committing to
 * the 128 KiB tier.
 *
 * Layout / feature knobs (LV_USE_FLEX/GRID, LV_USE_DEMO_CELLPHONE,
 * LV_FONT_DEFAULT) stay aligned with the simulator because they are
 * functional requirements. Cache budgets, however, are where the target
 * build should diverge first when SRAM matters.
 */

#ifndef LV_CONF_MCU_H
#define LV_CONF_MCU_H

#define LV_COLOR_DEPTH 16
#define LV_MEM_SIZE (76 * 1024)

#define LV_USE_LOG 0

#define LV_USE_FONT_VEC 1
/* MCU target: favor SRAM over simulator smoothness.
 * 2 KiB L2 keeps the hot digit/UI glyphs resident often enough for the
 * cellphone demo while halving the per-instance vec cache budget versus
 * the SDL config. L1 is also trimmed because the shipping path is
 * narrower than the desktop validation workload. If target profiling
 * shows churn on long SMS threads or larger alphabets, raise these again. */
#define LV_FONT_VEC_CACHE_SIZE 2048
#define LV_FONT_VEC_CACHE_L1_SETS 8
#define LV_DEMO_CELLPHONE_FONT_CACHE_SM 1536
#define LV_DEMO_CELLPHONE_FONT_CACHE_NORMAL 1536
#define LV_DEMO_CELLPHONE_FONT_CACHE_LARGE 1024

#define LV_FONT_MONTSERRAT_14 0
struct _lv_font_t;
extern const struct _lv_font_t lv_font_vec_stub;
#define LV_FONT_DEFAULT ((const lv_font_t *)&lv_font_vec_stub)

#define LV_USE_SDL 0
#define LV_USE_SNAPSHOT 0

#define LV_USE_FLEX 1
#define LV_USE_GRID 1
#define LV_GRADIENT_MAX_STOPS 2

#define LV_BUILD_DEMOS 1
#define LV_USE_DEMO_CELLPHONE 1
#define LV_DEMO_CELLPHONE_SKIN 0

#endif /* LV_CONF_MCU_H */
