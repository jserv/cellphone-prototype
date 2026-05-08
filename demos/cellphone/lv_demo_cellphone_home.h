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

#endif
#ifdef __cplusplus
}
#endif
#endif
