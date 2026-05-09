/**
 * @file lv_demo_cellphone_skin.h
 *
 * Device skin system for the Cell Phone Demo.
 * Renders a phone body frame around the LCD area, inspired by PicoGUI's
 * three-file skin model (device photo + conf + button map).
 *
 * Guarded by LV_DEMO_CELLPHONE_SKIN (must also have LV_USE_SDL).
 */

#ifndef LV_DEMO_CELLPHONE_SKIN_H
#define LV_DEMO_CELLPHONE_SKIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lv_demo_cellphone_common.h"

#if LV_USE_DEMO_CELLPHONE && defined(LV_DEMO_CELLPHONE_SKIN) && LV_DEMO_CELLPHONE_SKIN && LV_USE_SDL

/*********************
 *      INCLUDES
 *********************/
#include "../../lvgl.h"

/*********************
 *      DEFINES
 *********************/

/** Demo-specific key codes for hardware button hotspots.
 *  Modeled after PicoGUI keycodes in .map files. */
#define CELLPHONE_KEY_HOME      0x0100
#define CELLPHONE_KEY_POWER     0x0101

/**********************
 *      TYPEDEFS
 **********************/

/**
 * Clickable hardware-button hotspot on the skin image.
 * Coordinates are in skin-image space (not LCD space).
 * Modeled after PicoGUI's HTML image-map rectangles.
 */
typedef struct {
    int16_t  x1, y1, x2, y2; /* rectangle in skin image coords */
    uint32_t key;             /* LV_KEY_* or demo-specific code */
} cellphone_hotspot_t;

/**
 * Device skin descriptor.
 * Each skin is a static const instance + optional pre-converted image.
 * If frame_img is NULL, a procedural bezel is drawn instead.
 */
typedef struct {
    const char       *       name;      /* human-readable skin name         */
    const lv_image_dsc_t  * frame_img; /* pre-converted phone body, or NULL*/
    int16_t     lcd_x;       /* LCD region X offset in frame       */
    int16_t     lcd_y;       /* LCD region Y offset in frame       */
    uint16_t    lcd_w;       /* LCD width  (must match display)    */
    uint16_t    lcd_h;       /* LCD height (must match display)    */
    uint16_t    frame_w;     /* full skin image width              */
    uint16_t    frame_h;     /* full skin image height             */
    lv_color_t  bg_color;    /* fallback / bezel color             */
    lv_color_t  bezel_color; /* inner bezel accent color           */
    const cellphone_hotspot_t * hotspots;   /* counted array, or NULL       */
    uint16_t    hotspot_count;              /* number of entries in hotspots*/
} cellphone_skin_t;

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**
 * Get the number of registered skins.
 */
uint32_t cellphone_skin_count(void);

/**
 * Get a skin descriptor by index.
 * Returns NULL if idx is out of range.
 */
const cellphone_skin_t * cellphone_skin_get(uint32_t idx);

/**
 * Get the currently active skin index.
 */
uint32_t cellphone_skin_active_index(void);

/**
 * Get the currently active skin descriptor.
 */
const cellphone_skin_t * cellphone_skin_active(void);

/**
 * Get the window (frame) dimensions for the active skin.
 * Used by main_sdl.c to size the SDL window.
 */
void cellphone_skin_get_window_size(int32_t * w, int32_t * h);

/**
 * Create the skin layer inside the demo root.
 * Called once during demo init. Draws the phone body frame
 * and returns the LCD container where demo content goes.
 *
 * @param parent  The demo root object (full window size).
 * @return The LCD-sized container positioned at (lcd_x, lcd_y).
 */
lv_obj_t * cellphone_skin_create(lv_obj_t * parent);

/**
 * Switch to a different skin at runtime.
 * Redraws the frame, repositions the LCD container.
 *
 * @param idx  Skin index (0..cellphone_skin_count()-1).
 */
void cellphone_skin_set(uint32_t idx);

/**
 * Reset skin module state. Must be called before deleting the
 * demo root to prevent stale object pointers.
 */
void cellphone_skin_reset(void);

/**
 * LCD display tint preset.
 * Simulates display characteristics (green-tinted monochrome, warm OLED, etc.).
 * Modeled after PicoGUI's per-pixel tint (sdlfb_tint_pgtohwr).
 */
typedef struct {
    const char * name;
    lv_color_t   color;
    lv_opa_t     opa;
} cellphone_tint_preset_t;

/**
 * Get the number of built-in tint presets.
 */
uint32_t cellphone_tint_preset_count(void);

/**
 * Get a tint preset by index.
 */
const cellphone_tint_preset_t * cellphone_tint_preset_get(uint32_t idx);

/**
 * Apply a display tint to the LCD overlay.
 * Call with LV_OPA_TRANSP to disable tinting.
 */
void cellphone_skin_set_tint(lv_color_t color, lv_opa_t opa);

/**
 * Apply a tint preset by index.
 */
void cellphone_skin_set_tint_preset(uint32_t idx);

/**
 * Get the currently active tint preset index.
 */
uint32_t cellphone_tint_active_index(void);

#endif /* LV_USE_DEMO_CELLPHONE && LV_DEMO_CELLPHONE_SKIN && LV_USE_SDL */

#ifdef __cplusplus
}
#endif

#endif /* LV_DEMO_CELLPHONE_SKIN_H */
