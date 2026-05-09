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
#define VF_MARGIN 4
#define VF_RADIUS 8

/**********************
 *  STATIC PROTOTYPES
 **********************/
static lv_obj_t * overlay_chip_create(lv_obj_t * parent, const char * text,
                                      lv_color_t bg, lv_color_t fg);
static void focus_brackets_draw_cb(lv_event_t * e);
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
    int32_t vf_w = w - VF_MARGIN * 2;
    int32_t vf_h = h - CTRL_BAR_H - VF_MARGIN * 2;
    lv_obj_t * viewfinder = lv_obj_create(parent);
    lv_obj_set_size(viewfinder, vf_w, vf_h);
    lv_obj_set_pos(viewfinder, VF_MARGIN, VF_MARGIN);
    lv_obj_set_style_bg_color(viewfinder, lv_color_hex(0x90caf9), 0);
    lv_obj_set_style_bg_grad_color(viewfinder, lv_color_hex(0x1f2f3a), 0);
    lv_obj_set_style_bg_grad_dir(viewfinder, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_opa(viewfinder, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(viewfinder, VF_RADIUS, 0);
    lv_obj_set_style_border_color(viewfinder, lv_color_hex(0x455a64), 0);
    lv_obj_set_style_border_width(viewfinder, 1, 0);
    lv_obj_set_style_pad_all(viewfinder, 0, 0);
    lv_obj_clear_flag(viewfinder, LV_OBJ_FLAG_SCROLLABLE);

    /* Scenic scene layers: sky, distant ridge, near ridge, road, subject.
     * Geometry-only so the demo gets a richer camera preview without image assets. */
    lv_obj_t * ridge_far = cellphone_obj_fill(viewfinder, lv_color_hex(0x607d8b));
    lv_obj_set_size(ridge_far, vf_w + 24, vf_h / 3);
    lv_obj_align(ridge_far, LV_ALIGN_BOTTOM_MID, -8, -44);
    lv_obj_set_style_radius(ridge_far, 48, 0);
    lv_obj_remove_flag(ridge_far, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t * ridge_near = cellphone_obj_fill(viewfinder, lv_color_hex(0x455a64));
    lv_obj_set_size(ridge_near, vf_w + 28, vf_h / 4);
    lv_obj_align(ridge_near, LV_ALIGN_BOTTOM_MID, 10, -28);
    lv_obj_set_style_radius(ridge_near, 52, 0);
    lv_obj_remove_flag(ridge_near, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t * ground = cellphone_obj_fill(viewfinder, lv_color_hex(0x2e7d32));
    lv_obj_set_size(ground, vf_w, vf_h / 3);
    lv_obj_align(ground, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_radius(ground, 0, 0);
    lv_obj_remove_flag(ground, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t * road = cellphone_obj_fill(viewfinder, lv_color_hex(0x37474f));
    lv_obj_set_size(road, vf_w / 2, vf_h / 3 + 18);
    lv_obj_align(road, LV_ALIGN_BOTTOM_MID, 0, 18);
    /* Skew is degrees; tan(angle) is the per-row horizontal offset. Keep the
     * angle small or the bounding box explodes (e.g. 80deg ~= 5.6x height) and
     * forces a transform layer, which is expensive on the 76 KiB MCU profile. */
    lv_obj_set_style_transform_skew_x(road, -12, 0);
    lv_obj_set_style_radius(road, 10, 0);
    lv_obj_remove_flag(road, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t * road_line = cellphone_obj_fill(road, lv_color_hex(0xfff59d));
    lv_obj_set_size(road_line, 4, vf_h / 5);
    lv_obj_align(road_line, LV_ALIGN_TOP_MID, 0, 18);
    lv_obj_set_style_radius(road_line, LV_RADIUS_CIRCLE, 0);
    lv_obj_remove_flag(road_line, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t * subject = cellphone_obj_fill(viewfinder, lv_color_hex(0xff7043));
    lv_obj_set_size(subject, 34, 52);
    lv_obj_align(subject, LV_ALIGN_CENTER, 28, 24);
    lv_obj_set_style_radius(subject, 12, 0);
    lv_obj_remove_flag(subject, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t * subject_head = cellphone_obj_fill(subject, lv_color_hex(0xffcc80));
    lv_obj_set_size(subject_head, 18, 18);
    lv_obj_align(subject_head, LV_ALIGN_TOP_MID, 0, -8);
    lv_obj_set_style_radius(subject_head, LV_RADIUS_CIRCLE, 0);
    lv_obj_remove_flag(subject_head, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t * glare = cellphone_obj_bare(viewfinder);
    lv_obj_set_size(glare, vf_w / 2, vf_h - 24);
    lv_obj_align(glare, LV_ALIGN_LEFT_MID, 12, 0);
    lv_obj_set_style_bg_color(glare, lv_color_white(), 0);
    lv_obj_set_style_bg_grad_color(glare, lv_color_white(), 0);
    lv_obj_set_style_bg_grad_dir(glare, LV_GRAD_DIR_HOR, 0);
    lv_obj_set_style_bg_opa(glare, LV_OPA_10, 0);
    lv_obj_set_style_radius(glare, 18, 0);
    lv_obj_set_style_transform_skew_x(glare, -10, 0);
    lv_obj_set_style_border_width(glare, 0, 0);
    lv_obj_remove_flag(glare, LV_OBJ_FLAG_CLICKABLE);

    /* Top overlay chips */
    lv_obj_t * hdr = cellphone_obj_bare(viewfinder);
    lv_obj_set_size(hdr, vf_w - 16, LV_SIZE_CONTENT);
    lv_obj_align(hdr, LV_ALIGN_TOP_MID, 0, 8);
    lv_obj_set_flex_flow(hdr, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(hdr, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_remove_flag(hdr, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

    overlay_chip_create(hdr, "HDR", lv_color_hex(0x263238), lv_color_white());
    overlay_chip_create(hdr, "12 MP", lv_color_hex(0x263238), lv_color_white());

    /* Focus indicator: 4 L-shaped corner brackets painted in a single
     * draw event so the scene doesn't allocate 4 extra lv_obj_t for what
     * is purely chrome. */
    lv_obj_t * focus = cellphone_obj_transparent(viewfinder);
    lv_obj_set_size(focus, 48, 48);
    lv_obj_center(focus);
    lv_obj_remove_flag(focus, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(focus, focus_brackets_draw_cb, LV_EVENT_DRAW_MAIN, NULL);

    lv_obj_t * focus_dot = cellphone_obj_fill(focus, lv_color_hex(0xfff176));
    lv_obj_set_size(focus_dot, 6, 6);
    lv_obj_center(focus_dot);
    lv_obj_set_style_radius(focus_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_remove_flag(focus_dot, LV_OBJ_FLAG_CLICKABLE);

    /* Small EV label near focus */
    lv_obj_t * ev_lbl = cellphone_label(viewfinder, "EV -0.3",
                                        CELLPHONE_FONT_SM, lv_color_white());
    lv_obj_set_style_text_opa(ev_lbl, LV_OPA_80, 0);
    lv_obj_align_to(ev_lbl, focus, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);

    /* Flash overlay (initially transparent, flashes white on shutter press) */
    lv_obj_t * flash_overlay = lv_obj_create(viewfinder);
    lv_obj_set_size(flash_overlay, vf_w, vf_h);
    lv_obj_set_pos(flash_overlay, 0, 0);
    lv_obj_set_style_bg_color(flash_overlay, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(flash_overlay, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(flash_overlay, 0, 0);
    lv_obj_set_style_radius(flash_overlay, VF_RADIUS, 0);
    lv_obj_clear_flag(flash_overlay, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

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
static lv_obj_t * overlay_chip_create(lv_obj_t * parent, const char * text,
                                      lv_color_t bg, lv_color_t fg)
{
    lv_obj_t * chip = cellphone_obj_fill(parent, bg);
    lv_obj_set_size(chip, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(chip, LV_OPA_50, 0);
    lv_obj_set_style_radius(chip, 10, 0);
    lv_obj_set_style_pad_hor(chip, 8, 0);
    lv_obj_set_style_pad_ver(chip, 4, 0);
    lv_obj_remove_flag(chip, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    cellphone_label(chip, text, CELLPHONE_FONT_SM, fg);
    return chip;
}

static void focus_brackets_draw_cb(lv_event_t * e)
{
    lv_obj_t * obj = lv_event_get_target(e);
    lv_layer_t * layer = lv_event_get_layer(e);
    lv_area_t c;
    lv_obj_get_coords(obj, &c);

    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init(&dsc);
    dsc.bg_color = lv_color_white();
    dsc.bg_opa = LV_OPA_70;

    const int32_t leg = 14;
    const int32_t thick = 2;
    lv_area_t r;

    /* Top-left: horizontal stroke + vertical stroke */
    r.x1 = c.x1;
    r.y1 = c.y1;
    r.x2 = c.x1 + leg - 1;
    r.y2 = c.y1 + thick - 1;
    lv_draw_rect(layer, &dsc, &r);
    r.x1 = c.x1;
    r.y1 = c.y1;
    r.x2 = c.x1 + thick - 1;
    r.y2 = c.y1 + leg - 1;
    lv_draw_rect(layer, &dsc, &r);

    /* Top-right */
    r.x1 = c.x2 - leg + 1;
    r.y1 = c.y1;
    r.x2 = c.x2;
    r.y2 = c.y1 + thick - 1;
    lv_draw_rect(layer, &dsc, &r);
    r.x1 = c.x2 - thick + 1;
    r.y1 = c.y1;
    r.x2 = c.x2;
    r.y2 = c.y1 + leg - 1;
    lv_draw_rect(layer, &dsc, &r);

    /* Bottom-left */
    r.x1 = c.x1;
    r.y1 = c.y2 - thick + 1;
    r.x2 = c.x1 + leg - 1;
    r.y2 = c.y2;
    lv_draw_rect(layer, &dsc, &r);
    r.x1 = c.x1;
    r.y1 = c.y2 - leg + 1;
    r.x2 = c.x1 + thick - 1;
    r.y2 = c.y2;
    lv_draw_rect(layer, &dsc, &r);

    /* Bottom-right */
    r.x1 = c.x2 - leg + 1;
    r.y1 = c.y2 - thick + 1;
    r.x2 = c.x2;
    r.y2 = c.y2;
    lv_draw_rect(layer, &dsc, &r);
    r.x1 = c.x2 - thick + 1;
    r.y1 = c.y2 - leg + 1;
    r.x2 = c.x2;
    r.y2 = c.y2;
    lv_draw_rect(layer, &dsc, &r);
}

static void shutter_click_cb(lv_event_t * e)
{
    lv_obj_t * flash = (lv_obj_t *)lv_event_get_user_data(e);
    cellphone_anim_run(flash, cellphone_anim_set_bg_opa_cb,
                       LV_OPA_COVER, LV_OPA_TRANSP, 150,
                       lv_anim_path_ease_out);
}

#endif /* LV_USE_DEMO_CELLPHONE */
