/**
 * @file lv_demo_cellphone_anim.c
 *
 * Shared animation helpers.
 */

#include "lv_demo_cellphone_anim.h"

#if LV_USE_DEMO_CELLPHONE

/*********************
 *      DEFINES
 *********************/
#define PICKUP_DURATION_MS  120
#define PICKUP_SCALE_BASE   256  /* LV_SCALE_NONE = 1.0x */
#define PICKUP_SCALE_RAISED 284  /* ~1.11x */

/**********************
 *  STATIC PROTOTYPES
 **********************/

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

void cellphone_anim_pickup(lv_obj_t * obj)
{
    cellphone_anim_run(obj, cellphone_anim_set_scale_cb,
                       PICKUP_SCALE_BASE, PICKUP_SCALE_RAISED,
                       PICKUP_DURATION_MS, lv_anim_path_ease_out);
}

void cellphone_anim_drop(lv_obj_t * obj)
{
    cellphone_anim_run(obj, cellphone_anim_set_scale_cb,
                       PICKUP_SCALE_RAISED, PICKUP_SCALE_BASE,
                       PICKUP_DURATION_MS, lv_anim_path_ease_out);
}

#endif /* LV_USE_DEMO_CELLPHONE */
