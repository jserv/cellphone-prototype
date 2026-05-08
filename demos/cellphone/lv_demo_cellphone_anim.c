/**
 * @file lv_demo_cellphone_anim.c
 *
 * Shared animation helpers: wobble, pickup, highlight sweep.
 */

#include "lv_demo_cellphone_anim.h"

#if LV_USE_DEMO_CELLPHONE

/*********************
 *      DEFINES
 *********************/
#define WOBBLE_PERIOD_MS    400
#define WOBBLE_TRANSLATE_PX 3
#define WOBBLE_ROTATE_DEG10 20   /* 2.0 degrees in 0.1-deg units */

#define PICKUP_DURATION_MS  75
#define PICKUP_SCALE_BASE   256  /* LV_SCALE_NONE = 1.0x */
#define PICKUP_SCALE_RAISED 333  /* ~1.3x */
#define PICKUP_OPA_FULL     255
#define PICKUP_OPA_DIMMED   128

#define SWEEP_DURATION_MS   2000
#define SWEEP_PAUSE_MS      1000

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void wobble_x_cb(void * var, int32_t val);
static void sweep_x_cb(void * var, int32_t val);
static void rotation_cb(void * var, int32_t val);

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void cellphone_anim_set_x_cb(void * var, int32_t v)
{
    lv_obj_set_x((lv_obj_t *)var, v);
}

void cellphone_anim_set_y_cb(void * var, int32_t v)
{
    lv_obj_set_y((lv_obj_t *)var, v);
}

void cellphone_anim_set_width_cb(void * var, int32_t v)
{
    lv_obj_set_width((lv_obj_t *)var, v);
}

void cellphone_anim_set_height_cb(void * var, int32_t v)
{
    lv_obj_set_height((lv_obj_t *)var, v);
}

void cellphone_anim_set_opa_cb(void * var, int32_t v)
{
    lv_obj_set_style_opa((lv_obj_t *)var, (lv_opa_t)v, 0);
}

void cellphone_anim_set_bg_opa_cb(void * var, int32_t v)
{
    lv_obj_set_style_bg_opa((lv_obj_t *)var, (lv_opa_t)v, 0);
}

void cellphone_anim_set_scale_cb(void * var, int32_t v)
{
    lv_obj_set_style_transform_scale((lv_obj_t *)var, v, 0);
}

void cellphone_anim_run(lv_obj_t * obj, lv_anim_exec_xcb_t exec_cb,
                        int32_t v0, int32_t v1, uint32_t dur,
                        lv_anim_path_cb_t path)
{
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_values(&a, v0, v1);
    lv_anim_set_duration(&a, dur);
    lv_anim_set_exec_cb(&a, exec_cb);
    lv_anim_set_path_cb(&a, path);
    lv_anim_start(&a);
}

void cellphone_anim_wobble_start(lv_obj_t * obj, uint32_t delay_ms)
{
    lv_anim_t a;

    /* Translate X oscillation */
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_values(&a, -WOBBLE_TRANSLATE_PX, WOBBLE_TRANSLATE_PX);
    lv_anim_set_duration(&a, WOBBLE_PERIOD_MS);
    lv_anim_set_delay(&a, delay_ms);
    lv_anim_set_exec_cb(&a, wobble_x_cb);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_set_playback_duration(&a, WOBBLE_PERIOD_MS);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a);

    /* Rotation oscillation -- same envelope as the translate, just different cb */
    lv_anim_set_values(&a, -WOBBLE_ROTATE_DEG10, WOBBLE_ROTATE_DEG10);
    lv_anim_set_exec_cb(&a, rotation_cb);
    lv_anim_start(&a);
}

void cellphone_anim_wobble_stop(lv_obj_t * obj)
{
    /* Match the exec cb so we only stop the wobble animation, not any
     * concurrent sweep that happens to use a translate_x as well. */
    lv_anim_delete(obj, wobble_x_cb);
    lv_anim_delete(obj, rotation_cb);
    lv_obj_set_style_translate_x(obj, 0, 0);
    lv_obj_set_style_transform_rotation(obj, 0, 0);
}

void cellphone_anim_pickup(lv_obj_t * obj)
{
    cellphone_anim_run(obj, cellphone_anim_set_scale_cb,
                       PICKUP_SCALE_BASE, PICKUP_SCALE_RAISED,
                       PICKUP_DURATION_MS, lv_anim_path_ease_out);
    cellphone_anim_run(obj, cellphone_anim_set_opa_cb,
                       PICKUP_OPA_FULL, PICKUP_OPA_DIMMED,
                       PICKUP_DURATION_MS, lv_anim_path_ease_out);
}

void cellphone_anim_drop(lv_obj_t * obj)
{
    /* Mirror the easing used by cellphone_anim_pickup so a touch + release
     * pair feels symmetric -- ease_out on both legs gives the standard
     * "settles into place" curve. */
    cellphone_anim_run(obj, cellphone_anim_set_scale_cb,
                       PICKUP_SCALE_RAISED, PICKUP_SCALE_BASE,
                       PICKUP_DURATION_MS, lv_anim_path_ease_out);
    cellphone_anim_run(obj, cellphone_anim_set_opa_cb,
                       PICKUP_OPA_DIMMED, PICKUP_OPA_FULL,
                       PICKUP_DURATION_MS, lv_anim_path_ease_out);
}

void cellphone_anim_highlight_sweep(lv_obj_t * highlight, int32_t width)
{
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, highlight);
    lv_anim_set_values(&a, -30, width + 30);
    lv_anim_set_duration(&a, SWEEP_DURATION_MS);
    lv_anim_set_exec_cb(&a, sweep_x_cb);
    lv_anim_set_path_cb(&a, lv_anim_path_linear);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_repeat_delay(&a, SWEEP_PAUSE_MS);
    lv_anim_start(&a);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/* Two distinct callbacks both setting translate_x: this lets
 * lv_anim_delete(obj, cb) target wobble vs. sweep independently. */
static void wobble_x_cb(void * var, int32_t val)
{
    lv_obj_set_style_translate_x((lv_obj_t *)var, val, 0);
}

static void sweep_x_cb(void * var, int32_t val)
{
    lv_obj_set_style_translate_x((lv_obj_t *)var, val, 0);
}

static void rotation_cb(void * var, int32_t val)
{
    lv_obj_set_style_transform_rotation((lv_obj_t *)var, val, 0);
}

#endif /* LV_USE_DEMO_CELLPHONE */
