/**
 * @file lv_demo_cellphone_contacts.h
 */

#ifndef LV_DEMO_CELLPHONE_CONTACTS_H
#define LV_DEMO_CELLPHONE_CONTACTS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lv_demo_cellphone_common.h"

#if LV_USE_DEMO_CELLPHONE

lv_obj_t * cellphone_contacts_create(lv_obj_t * parent);
bool cellphone_contacts_test_open_add_overlay(void);
lv_obj_t * cellphone_contacts_test_get_add_field(uint32_t idx);
bool cellphone_contacts_test_advance_add_focus(uint32_t from_idx);

#endif
#ifdef __cplusplus
}
#endif
#endif
