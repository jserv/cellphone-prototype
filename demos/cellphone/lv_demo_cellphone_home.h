/**
 * @file lv_demo_cellphone_home.h
 */

#ifndef LV_DEMO_CELLPHONE_HOME_H
#define LV_DEMO_CELLPHONE_HOME_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lv_demo_cellphone_common.h"

#if LV_USE_DEMO_CELLPHONE

/**
 * Create the home screen launcher grid inside the given content area.
 */
lv_obj_t * cellphone_home_create(lv_obj_t * parent);
uint32_t cellphone_home_refresh_state_capture(void);
void cellphone_home_refresh_state_restore(uint32_t state);

/* Procedural icon glyphs for apps the bundled FA5 subset can't cover.
 * Each paints a white silhouette inside `plate`, mixing `accent` for
 * inner contrast tints. Coordinates are sized for the QVGA 52 px
 * plate; the body scales acceptably to 68 px in large mode because
 * everything is drawn relative to the plate center. */
void cellphone_glyph_messages(lv_layer_t * layer, const lv_area_t * plate, lv_color_t accent);
void cellphone_glyph_calculator(lv_layer_t * layer, const lv_area_t * plate, lv_color_t accent);
void cellphone_glyph_camera(lv_layer_t * layer, const lv_area_t * plate, lv_color_t accent);
void cellphone_glyph_snake(lv_layer_t * layer, const lv_area_t * plate, lv_color_t accent);
void cellphone_glyph_pong(lv_layer_t * layer, const lv_area_t * plate, lv_color_t accent);
void cellphone_glyph_tetris(lv_layer_t * layer, const lv_area_t * plate, lv_color_t accent);

#endif
#ifdef __cplusplus
}
#endif
#endif
