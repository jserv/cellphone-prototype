/**
 * @file lv_demo_cellphone_data.h
 *
 * Static demo datasets: contacts, SMS, call log, music tracks.
 * All data is compiled-in, no filesystem or SQLite required.
 */

#ifndef LV_DEMO_CELLPHONE_DATA_H
#define LV_DEMO_CELLPHONE_DATA_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lv_demo_cellphone_common.h"

#if LV_USE_DEMO_CELLPHONE

/*********************
 * CONTACTS
 *********************/
typedef struct {
    const char * name;
    const char * phone;
    const char * email;
    char initial;           /* first letter, for section headers */
} cellphone_contact_t;

#define CELLPHONE_CONTACT_COUNT 12

const cellphone_contact_t * cellphone_data_contacts(void);

/*********************
 * SMS
 *********************/
typedef struct {
    const char * text;
    uint8_t is_sent;        /* 1 = sent by user, 0 = received */
    const char * time;      /* display time string, e.g. "10:30" */
} cellphone_sms_msg_t;

typedef struct {
    const char * contact_name;
    const char * preview;       /* last message preview */
    const char * timestamp;     /* e.g. "Yesterday" */
    const cellphone_sms_msg_t * messages;
    uint32_t msg_count;
} cellphone_sms_thread_t;

#define CELLPHONE_SMS_THREAD_COUNT 4

const cellphone_sms_thread_t * cellphone_data_sms_threads(void);

/*********************
 * CALL LOG
 *********************/
typedef enum {
    CELLPHONE_CALL_INCOMING = 0,
    CELLPHONE_CALL_OUTGOING,
    CELLPHONE_CALL_MISSED,
} cellphone_call_dir_t;

typedef struct {
    const char * name;
    const char * phone;
    const char * time;          /* e.g. "Today, 14:22" */
    cellphone_call_dir_t dir;
} cellphone_calllog_entry_t;

#define CELLPHONE_CALLLOG_COUNT 15

const cellphone_calllog_entry_t * cellphone_data_calllog(void);

/*********************
 * MUSIC
 *********************/
typedef struct {
    const char * title;
    const char * artist;
    const char * album;
    uint32_t duration_sec;
} cellphone_track_t;

#define CELLPHONE_TRACK_COUNT 10

const cellphone_track_t * cellphone_data_tracks(void);

#endif /* LV_USE_DEMO_CELLPHONE */

#ifdef __cplusplus
}
#endif

#endif /* LV_DEMO_CELLPHONE_DATA_H */
