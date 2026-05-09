/**
 * @file lv_demo_cellphone_camera.h
 */

#ifndef LV_DEMO_CELLPHONE_CAMERA_H
#define LV_DEMO_CELLPHONE_CAMERA_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lv_demo_cellphone_common.h"

#if LV_USE_DEMO_CELLPHONE

/**
 * Create the camera app inside the given content area.
 */
lv_obj_t * cellphone_camera_create(lv_obj_t * parent);

#endif /* LV_USE_DEMO_CELLPHONE */

#ifdef __cplusplus
}
#endif

#endif /* LV_DEMO_CELLPHONE_CAMERA_H */
