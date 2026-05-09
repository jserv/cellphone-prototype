/**
 * @file lv_demo_cellphone_calc.h
 */

#ifndef LV_DEMO_CELLPHONE_CALC_H
#define LV_DEMO_CELLPHONE_CALC_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lv_demo_cellphone_common.h"

#if LV_USE_DEMO_CELLPHONE

/**
 * Create the calculator app inside the given content area.
 */
lv_obj_t * cellphone_calc_create(lv_obj_t * parent);

#endif /* LV_USE_DEMO_CELLPHONE */

#ifdef __cplusplus
}
#endif

#endif /* LV_DEMO_CELLPHONE_CALC_H */
