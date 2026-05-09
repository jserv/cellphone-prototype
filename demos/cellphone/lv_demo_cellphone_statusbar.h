/**
 * @file lv_demo_cellphone_statusbar.h
 */

#ifndef LV_DEMO_CELLPHONE_STATUSBAR_H
#define LV_DEMO_CELLPHONE_STATUSBAR_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lv_demo_cellphone_common.h"

#if LV_USE_DEMO_CELLPHONE

/**
 * Create the status bar on the top layer so it persists across screens.
 * @param layer  Typically lv_layer_top().
 */
void cellphone_statusbar_create(lv_obj_t * layer);

#endif
#ifdef __cplusplus
}
#endif
#endif
