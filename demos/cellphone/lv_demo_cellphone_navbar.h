/**
 * @file lv_demo_cellphone_navbar.h
 */

#ifndef LV_DEMO_CELLPHONE_NAVBAR_H
#define LV_DEMO_CELLPHONE_NAVBAR_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lv_demo_cellphone_common.h"

#if LV_USE_DEMO_CELLPHONE

/**
 * Create the bottom navigation bar on the top layer.
 * @param layer  Typically lv_layer_top().
 */
void cellphone_navbar_create(lv_obj_t * layer);

/**
 * Refresh the navbar's active-tab highlight to match the current top screen.
 * Called by the screen stack on every push/pop/home transition.
 */
void cellphone_navbar_refresh(void);

/**
 * Dispatch a click event to a navbar tab.
 * Useful for automated tests that need to validate tab behavior without
 * depending on pixel-level hit coordinates.
 *
 * @param index  Zero-based tab index: 0 = Dialer, 1 = middle tab, 2 = Contacts.
 * @return       true when the tab exists and the click event was sent.
 */
bool cellphone_navbar_tab_click(uint32_t index);

#endif
#ifdef __cplusplus
}
#endif
#endif
