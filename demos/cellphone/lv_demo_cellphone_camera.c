/**
 * @file lv_demo_cellphone_camera.c
 *
 * Camera app: simulated viewfinder with shutter button and mode selector.
 */

#include "lv_demo_cellphone_camera.h"
#include "lv_demo_cellphone_anim.h"

#if LV_USE_DEMO_CELLPHONE

/*********************
 *      DEFINES
 *********************/
#define CTRL_BAR_H  52
#define SHUTTER_SIZE 44

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void shutter_click_cb(lv_event_t * e);

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

lv_obj_t * cellphone_camera_create(lv_obj_t * parent)
{
    int32_t w = CELLPHONE_CONTENT_W;
    int32_t h = CELLPHONE_CONTENT_H;

    /* Black background */
    cellphone_obj_paint_fill(parent, lv_color_black());

    /* --- Viewfinder area --- */
    lv_obj_t * viewfinder = lv_obj_create(parent);
    lv_obj_set_size(viewfinder, w - 8, h - CTRL_BAR_H - 8);
    lv_obj_set_pos(viewfinder, 4, 4);
    lv_obj_set_style_bg_color(viewfinder, lv_color_hex(0x263238), 0);
    lv_obj_set_style_bg_grad_color(viewfinder, lv_color_hex(0x1b2631), 0);
    lv_obj_set_style_bg_grad_dir(viewfinder, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_opa(viewfinder, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(viewfinder, 8, 0);
    lv_obj_set_style_border_color(viewfinder, lv_color_hex(0x455a64), 0);
    lv_obj_set_style_border_width(viewfinder, 1, 0);
    lv_obj_set_style_pad_all(viewfinder, 0, 0);
    lv_obj_clear_flag(viewfinder, LV_OBJ_FLAG_SCROLLABLE);

    /* Focus indicator */
    lv_obj_t * focus = lv_obj_create(viewfinder);
    lv_obj_set_size(focus, 40, 40);
    lv_obj_center(focus);
    lv_obj_set_style_bg_opa(focus, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(focus, lv_color_white(), 0);
    lv_obj_set_style_border_width(focus, 1, 0);
    lv_obj_set_style_border_opa(focus, LV_OPA_60, 0);
    lv_obj_set_style_radius(focus, 4, 0);

    /* "Camera" label */
    lv_obj_t * cam_lbl = cellphone_label(viewfinder, "Camera",
                                         CELLPHONE_FONT_SM, lv_color_white());
    lv_obj_set_style_text_opa(cam_lbl, LV_OPA_50, 0);
    lv_obj_align(cam_lbl, LV_ALIGN_TOP_LEFT, 8, 8);

    /* Flash overlay (initially transparent, flashes white on shutter press) */
    lv_obj_t * flash_overlay = lv_obj_create(viewfinder);
    lv_obj_set_size(flash_overlay, w - 8, h - CTRL_BAR_H - 8);
    lv_obj_set_pos(flash_overlay, 0, 0);
    lv_obj_set_style_bg_color(flash_overlay, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(flash_overlay, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(flash_overlay, 0, 0);
    lv_obj_clear_flag(flash_overlay, LV_OBJ_FLAG_CLICKABLE);

    /* --- Bottom control bar --- */
    lv_obj_t * ctrl_bar = cellphone_obj_fill(parent, lv_color_hex(0x1a1a1a));
    lv_obj_set_size(ctrl_bar, w, CTRL_BAR_H);
    lv_obj_set_pos(ctrl_bar, 0, h - CTRL_BAR_H);
    lv_obj_set_style_pad_all(ctrl_bar, 4, 0);
    lv_obj_clear_flag(ctrl_bar, LV_OBJ_FLAG_SCROLLABLE);

    /* "Photo" mode label */
    lv_obj_t * mode_lbl = cellphone_label(ctrl_bar, "Photo",
                                          CELLPHONE_FONT_SM, lv_color_white());
    lv_obj_align(mode_lbl, LV_ALIGN_LEFT_MID, 12, 0);

    /* Shutter button: white circle, centered */
    lv_obj_t * shutter = lv_button_create(ctrl_bar);
    lv_obj_set_size(shutter, SHUTTER_SIZE, SHUTTER_SIZE);
    lv_obj_set_style_radius(shutter, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(shutter, lv_color_white(), 0);
    lv_obj_set_style_border_color(shutter, lv_color_hex(0xcccccc), 0);
    lv_obj_set_style_border_width(shutter, 3, 0);
    lv_obj_set_style_shadow_width(shutter, 0, 0);
    lv_obj_align(shutter, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_event_cb(shutter, shutter_click_cb, LV_EVENT_CLICKED, flash_overlay);

    /* "Video" mode label */
    lv_obj_t * video_lbl = cellphone_label(ctrl_bar, "Video",
                                           CELLPHONE_FONT_SM, lv_color_hex(0x888888));
    lv_obj_align(video_lbl, LV_ALIGN_RIGHT_MID, -12, 0);

    return parent;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/**
 * E18: Shutter flash effect -- white overlay fade, 150ms.
 */
static void shutter_click_cb(lv_event_t * e)
{
    lv_obj_t * flash = (lv_obj_t *)lv_event_get_user_data(e);
    cellphone_anim_run(flash, cellphone_anim_set_bg_opa_cb,
                       LV_OPA_COVER, LV_OPA_TRANSP, 150,
                       lv_anim_path_ease_out);
}

#endif /* LV_USE_DEMO_CELLPHONE */
