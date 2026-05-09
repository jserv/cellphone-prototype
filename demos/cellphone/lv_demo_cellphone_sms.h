/**
 * @file lv_demo_cellphone_sms.h
 */

#ifndef LV_DEMO_CELLPHONE_SMS_H
#define LV_DEMO_CELLPHONE_SMS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lv_demo_cellphone_common.h"

#if LV_USE_DEMO_CELLPHONE

lv_obj_t * cellphone_sms_create(lv_obj_t * parent);

/**
 * Open the chat detail view for a contact by name.  If the contact has
 * an existing thread it opens with the full message history; otherwise
 * a fresh empty chat is presented.  @p name must remain valid for the
 * lifetime of the screen (compiled-in strings are fine).
 */
void cellphone_sms_open_chat_with(const char * name);

#endif
#ifdef __cplusplus
}
#endif
#endif
