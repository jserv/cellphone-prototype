/**
 * @file lv_demo_cellphone_anim.h
 *
 * Shared animation helpers for the Cell Phone Demo.
 */

#ifndef LV_DEMO_CELLPHONE_ANIM_H
#define LV_DEMO_CELLPHONE_ANIM_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lv_demo_cellphone_common.h"

#if LV_USE_DEMO_CELLPHONE

/**
 * Start icon wobble animation on an object (edit mode).
 * Oscillates translate_x (-3..+3 px) and rotation (-20..+20 0.1deg units).
 * @param obj        Target object.
 * @param delay_ms   Stagger delay for organic feel.
 */
void cellphone_anim_wobble_start(lv_obj_t * obj, uint32_t delay_ms);

/**
 * Stop wobble animation on an object and reset transforms.
 */
void cellphone_anim_wobble_stop(lv_obj_t * obj);

/**
 * Scale-and-fade pickup effect (icon selected for dragging).
 * Scale 256->333 (1.0x->1.3x), opacity 255->128, duration 75ms.
 */
void cellphone_anim_pickup(lv_obj_t * obj);

/**
 * Reverse pickup: scale 333->256, opacity 128->255, duration 75ms.
 */
void cellphone_anim_drop(lv_obj_t * obj);

/**
 * Highlight sweep: animate a child object's X position across a label width.
 * Used for "slide to unlock" text highlight.
 * @param highlight  The highlight overlay object.
 * @param width      Total sweep distance in pixels.
 */
void cellphone_anim_highlight_sweep(lv_obj_t * highlight, int32_t width);

/* Shared single-line setter callbacks for animations across the demo.
 * Sharing folds away ~5 duplicated wrappers and lets `lv_anim_delete(obj, cb)`
 * target the same cb regardless of which screen started the anim. Each
 * cb keys uniquely off (obj, fn-ptr), so reuse across distinct objects is
 * safe; reuse on the same object cancels the prior anim, which is what
 * every caller already relies on. */
void cellphone_anim_set_x_cb(void * var, int32_t v);
void cellphone_anim_set_y_cb(void * var, int32_t v);
void cellphone_anim_set_width_cb(void * var, int32_t v);
void cellphone_anim_set_height_cb(void * var, int32_t v);
void cellphone_anim_set_opa_cb(void * var, int32_t v);
void cellphone_anim_set_bg_opa_cb(void * var, int32_t v);
void cellphone_anim_set_scale_cb(void * var, int32_t v);

/**
 * One-shot animation helper: var, exec, v0..v1 over `dur` ms with `path`.
 * Replaces the 6-line lv_anim_init/var/values/duration/exec/path/start
 * recipe scattered across screens. For animations that need delays,
 * playback, repeat, or completed_cb, build the lv_anim_t inline.
 */
void cellphone_anim_run(lv_obj_t * obj, lv_anim_exec_xcb_t exec_cb,
                        int32_t v0, int32_t v1, uint32_t dur,
                        lv_anim_path_cb_t path);

#endif /* LV_USE_DEMO_CELLPHONE */

#ifdef __cplusplus
}
#endif
#endif
