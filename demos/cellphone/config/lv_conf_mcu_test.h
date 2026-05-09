/**
 * @file lv_conf_mcu_test.h
 *
 * Host-test profile for validating the cellphone demo with the built-in
 * SDL harness. Keeps the MCU-oriented feature choices, but leaves extra
 * LVGL pool headroom so correctness validation is not blocked by the
 * host-side tester's broader redraw workload.
 */

#ifndef LV_CONF_MCU_TEST_H
#define LV_CONF_MCU_TEST_H

#define LV_COLOR_DEPTH 16
#define LV_MEM_SIZE (128 * 1024)

#define LV_USE_LOG 0

#define LV_USE_FONT_VEC 1
#define LV_FONT_VEC_CACHE_SIZE 2048
#define LV_FONT_VEC_CACHE_L1_SETS 8
#define LV_DEMO_CELLPHONE_FONT_CACHE_SM 1536
#define LV_DEMO_CELLPHONE_FONT_CACHE_NORMAL 1536
#define LV_DEMO_CELLPHONE_FONT_CACHE_LARGE 1024

#define LV_FONT_MONTSERRAT_14 0
struct _lv_font_t;
extern const struct _lv_font_t lv_font_vec_stub;
#define LV_FONT_DEFAULT ((const lv_font_t *)&lv_font_vec_stub)

#define LV_USE_SDL 1
#define LV_SDL_INCLUDE_PATH <SDL.h>
#define LV_SDL_RENDER_MODE LV_DISPLAY_RENDER_MODE_FULL
#define LV_SDL_BUF_COUNT 1
#define LV_USE_SNAPSHOT 1

#define LV_USE_TJPGD 1
#define LV_USE_FS_STDIO 1
#define LV_FS_STDIO_LETTER 'P'
#define LV_DEMO_CELLPHONE_PHOTOS_SOURCE 1

#define LV_USE_FLEX 1
#define LV_USE_GRID 1
#define LV_GRADIENT_MAX_STOPS 2

#define LV_BUILD_DEMOS 1
#define LV_USE_DEMO_CELLPHONE 1
#define LV_DEMO_CELLPHONE_SKIN 1

#endif /* LV_CONF_MCU_TEST_H */
