/**
 * @file lv_demo_cellphone.h
 *
 * Cell Phone Demo
 */

#ifndef LV_DEMO_CELLPHONE_H
#define LV_DEMO_CELLPHONE_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#include "../lv_demos.h"

#if LV_USE_DEMO_CELLPHONE

/*********************
 *      DEFINES
 *********************/

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**
 * Start the Cell Phone Demo on the active screen.
 */
void lv_demo_cellphone(void);

/**
 * Start the Cell Phone Demo with custom arguments.
 * @param args  Demo arguments (parent widget, etc.).
 */
void lv_demo_cellphone_with_args(const lv_demo_args_t * args);

/**
 * Rebuild the Cell Phone Demo using the last arguments passed in.
 */
void lv_demo_cellphone_rebuild(void);

/**
 * Rebuild the demo in-place while preserving the current screen stack.
 * Used when a theme change should take effect immediately without
 * jumping back to the lock screen.
 */
void lv_demo_cellphone_refresh_theme(void);

/**********************
 *      MACROS
 **********************/

#endif /* LV_USE_DEMO_CELLPHONE */

#ifdef __cplusplus
}
#endif

#endif /* LV_DEMO_CELLPHONE_H */
