/**
 * @file main_sdl.c
 *
 * Minimal SDL2 host for the Cell Phone Demo. Lives in
 * demos/cellphone/host/ so its main() is excluded from liblvgl_demos.a
 * via the env_support/cmake/main.cmake host/ filter.
 *
 * Prefer the build driver:
 *   demos/cellphone/build.sh demo
 */

#include "lvgl.h"
#include "demos/lv_demos.h"
#include "demos/cellphone/lv_demo_cellphone_common.h"

#if defined(LV_DEMO_CELLPHONE_SKIN) && LV_DEMO_CELLPHONE_SKIN && LV_USE_SDL
    #include "demos/cellphone/lv_demo_cellphone_skin.h"
#endif

int main(int argc, char ** argv)
{
    (void)argc;
    (void)argv;

    lv_init();

    int32_t win_w = CELLPHONE_HOR_RES;
    int32_t win_h = CELLPHONE_VER_RES;

#if defined(LV_DEMO_CELLPHONE_SKIN) && LV_DEMO_CELLPHONE_SKIN && LV_USE_SDL
    /* Size the SDL window to the full skin frame (bezel + LCD) */
    cellphone_skin_get_window_size(&win_w, &win_h);
#endif

    lv_display_t * disp = lv_sdl_window_create(win_w, win_h);
    lv_sdl_window_set_title(disp, "Cell Phone Demo");

    /* Create an SDL mouse input device */
    lv_sdl_mouse_create();

    /* Launch the cellphone demo */
    lv_demo_cellphone();

    /* Main loop */
    for(;;) {
        uint32_t ms = lv_timer_handler();
        lv_delay_ms(ms);
    }

    return 0;
}
