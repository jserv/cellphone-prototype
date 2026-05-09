/**
 * @file lv_demo_cellphone_photo.h
 */

#ifndef LV_DEMO_CELLPHONE_PHOTO_H
#define LV_DEMO_CELLPHONE_PHOTO_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lv_demo_cellphone_common.h"

#if LV_USE_DEMO_CELLPHONE

#define CELLPHONE_PHOTO_SOURCE_AUTO      0
#define CELLPHONE_PHOTO_SOURCE_REAL_JPEG 1
#define CELLPHONE_PHOTO_SOURCE_DUMMY     2

lv_obj_t * cellphone_photo_create(lv_obj_t * parent);
uint32_t cellphone_photo_source_config(void);
bool cellphone_photo_real_jpeg_supported(void);
bool cellphone_photo_uses_real_jpeg(void);
bool cellphone_photo_is_available(void);
const char * cellphone_photo_source_name(void);

#endif
#ifdef __cplusplus
}
#endif
#endif
