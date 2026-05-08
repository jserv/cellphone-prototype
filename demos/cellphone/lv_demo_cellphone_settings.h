/**
 * @file lv_demo_cellphone_settings.h
 */

#ifndef LV_DEMO_CELLPHONE_SETTINGS_H
#define LV_DEMO_CELLPHONE_SETTINGS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lv_demo_cellphone_common.h"

#if LV_USE_DEMO_CELLPHONE

lv_obj_t * cellphone_settings_create(lv_obj_t * parent);
uint32_t cellphone_settings_refresh_state_capture(void);
void cellphone_settings_refresh_state_restore(uint32_t state);

#endif
#ifdef __cplusplus
}
#endif
#endif
