/**
 * @file lv_demo_cellphone_dialer.h
 */

#ifndef LV_DEMO_CELLPHONE_DIALER_H
#define LV_DEMO_CELLPHONE_DIALER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lv_demo_cellphone_common.h"

#if LV_USE_DEMO_CELLPHONE

/**
 * Create the phone dialer screen inside the given content area.
 */
lv_obj_t * cellphone_dialer_create(lv_obj_t * parent);

/**
 * Place a call to a known contact: pushes the in-call screen directly,
 * skipping the keypad. @p name may be NULL when only a number is known
 * (e.g. an unknown caller); in that case only the number is displayed.
 * The pointers must remain valid for the lifetime of the screen --
 * compiled-in strings (the demo's own data tables) are fine.
 */
void cellphone_dialer_call_contact(const char * name, const char * phone);

#endif

#ifdef __cplusplus
}
#endif
#endif /* LV_DEMO_CELLPHONE_DIALER_H */
