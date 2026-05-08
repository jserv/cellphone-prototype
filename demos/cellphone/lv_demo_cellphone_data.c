/**
 * @file lv_demo_cellphone_data.c
 *
 * Compiled-in demo datasets.  All const -- lives in flash on MCU targets.
 */

#include "lv_demo_cellphone_data.h"

#if LV_USE_DEMO_CELLPHONE

/*********************
 * CONTACTS
 *********************/
static const cellphone_contact_t s_contacts_stock[CELLPHONE_CONTACT_COUNT] = {
    { "Ashley Johnson",  "+1-555-0101", "ashley@example.com",   'A' },
    { "Brian Smith",     "+1-555-0102", "brian@example.com",    'B' },
    { "Chris Williams",  "+1-555-0103", "chris@example.com",    'C' },
    { "Daniel Brown",    "+1-555-0104", "daniel@example.com",   'D' },
    { "Emily Davis",     "+1-555-0105", "emily@example.com",    'E' },
    { "Frank Miller",    "+1-555-0106", "frank@example.com",    'F' },
    { "Grace Wilson",    "+1-555-0107", "grace@example.com",    'G' },
    { "Henry Thomas",    "+1-555-0108", "henry@example.com",    'H' },
    { "Isaac Moore",     "+1-555-0109", "isaac@example.com",    'I' },
    { "James Taylor",    "+1-555-0110", "james@example.com",    'J' },
    { "Karen Harris",    "+1-555-0111", "karen@example.com",    'K' },
    { "Laura Jackson",   "+1-555-0112", "laura@example.com",    'L' },
};

#define CONTACT_ADDED_MAX (CELLPHONE_CONTACT_CAPACITY - CELLPHONE_CONTACT_COUNT)
#define CONTACT_ORDER_ADDED 0x80u  /* high bit flags an added-slot index */

static cellphone_contact_t s_contacts_added[CONTACT_ADDED_MAX];
static char s_added_names[CONTACT_ADDED_MAX][32];
static char s_added_phones[CONTACT_ADDED_MAX][24];
static char s_added_emails[CONTACT_ADDED_MAX][40];

/* High bit of an order entry flags an added-slot index, so the visible
 * list stays sorted without copying stock rows into RAM. */
static uint8_t s_contact_order[CELLPHONE_CONTACT_CAPACITY];
static uint32_t s_contact_count;
static bool s_contacts_inited;

static void contacts_init(void)
{
    if(s_contacts_inited) return;

    for(uint32_t i = 0; i < CELLPHONE_CONTACT_COUNT; i++) {
        s_contact_order[i] = (uint8_t)i;
    }

    s_contact_count = CELLPHONE_CONTACT_COUNT;
    s_contacts_inited = true;
}

static const cellphone_contact_t * contact_resolve(uint8_t ord)
{
    if(ord & CONTACT_ORDER_ADDED) return &s_contacts_added[ord & 0x7Fu];
    return &s_contacts_stock[ord];
}

static int32_t contact_name_cmp_folded(const char * lhs, const char * rhs)
{
    uint32_t i = 0;

    while(lhs[i] != '\0' && rhs[i] != '\0') {
        char a = lhs[i];
        char b = rhs[i];

        if(a >= 'a' && a <= 'z') a = (char)(a - 'a' + 'A');
        if(b >= 'a' && b <= 'z') b = (char)(b - 'a' + 'A');

        if(a != b) return (int32_t)((unsigned char)a - (unsigned char)b);
        i++;
    }

    return (int32_t)((unsigned char)lhs[i] - (unsigned char)rhs[i]);
}

static char contact_initial_for(const char * name)
{
    if(name == NULL || name[0] == '\0') return '#';

    char c = name[0];
    if(c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
    if((c < 'A' || c > 'Z') && (c < '0' || c > '9')) return '#';
    return c;
}

const cellphone_contact_t * cellphone_data_contact_at(uint32_t idx)
{
    contacts_init();
    if(idx >= s_contact_count) return NULL;
    return contact_resolve(s_contact_order[idx]);
}

uint32_t cellphone_data_contact_count(void)
{
    contacts_init();
    return s_contact_count;
}

int32_t cellphone_data_contact_add(const char * name,
                                   const char * phone,
                                   const char * email)
{
    contacts_init();

    if(!name || name[0] == '\0') return -1;
    if(s_contact_count >= CELLPHONE_CONTACT_CAPACITY) return -1;
    if(!phone) phone = "";
    if(!email) email = "";

    uint32_t buf_idx = s_contact_count - CELLPHONE_CONTACT_COUNT;
    lv_snprintf(s_added_names[buf_idx], sizeof(s_added_names[buf_idx]), "%s", name);
    lv_snprintf(s_added_phones[buf_idx], sizeof(s_added_phones[buf_idx]), "%s", phone);
    lv_snprintf(s_added_emails[buf_idx], sizeof(s_added_emails[buf_idx]), "%s", email);

    s_contacts_added[buf_idx] = (cellphone_contact_t) {
        .name = s_added_names[buf_idx],
        .phone = s_added_phones[buf_idx],
        .email = s_added_emails[buf_idx],
        .initial = contact_initial_for(s_added_names[buf_idx]),
    };

    uint32_t insert_at = s_contact_count;
    for(uint32_t i = 0; i < s_contact_count; i++) {
        const cellphone_contact_t * existing = contact_resolve(s_contact_order[i]);
        if(contact_name_cmp_folded(s_contacts_added[buf_idx].name, existing->name) < 0) {
            insert_at = i;
            break;
        }
    }

    for(uint32_t i = s_contact_count; i > insert_at; i--) {
        s_contact_order[i] = s_contact_order[i - 1];
    }

    s_contact_order[insert_at] = (uint8_t)(CONTACT_ORDER_ADDED | buf_idx);
    s_contact_count++;
    return (int32_t)insert_at;
}

void cellphone_data_reset_mutable(void)
{
    s_contacts_inited = false;
    contacts_init();
}

/*********************
 * SMS
 *********************/
static const cellphone_sms_msg_t s_sms_ashley[] = {
    { "Hey, you coming to the meeting?",        0, "09:15" },
    { "Yeah, on my way!",                       1, "09:17" },
    { "Cool, see you in 5",                     0, "09:18" },
    { "Just got here",                          1, "09:22" },
    { "Conference room B",                      0, "09:23" },
    { "Got it, thanks!",                        1, "09:24" },
};

static const cellphone_sms_msg_t s_sms_brian[] = {
    { "Did you see the new design specs?",      0, "14:05" },
    { "Not yet, got a link?",                   1, "14:10" },
    { "Sent it to your inbox",                  0, "14:11" },
    { "Thanks, looks great!",                   1, "14:30" },
    { "Want to chat about it tomorrow?",        0, "14:31" },
    { "Sure, mornings work for me",             1, "14:35" },
    { "10 AM then",                             0, "14:36" },
};

static const cellphone_sms_msg_t s_sms_chris[] = {
    { "Happy birthday!",                        1, "08:00" },
    { "Thanks so much!",                        0, "08:45" },
    { "Hope you have a great one",              1, "08:46" },
    { "We should grab dinner soon",             0, "08:50" },
    { "Sounds good, how about Friday?",         1, "08:52" },
};

static const cellphone_sms_msg_t s_sms_daniel[] = {
    { "Can you pick up groceries?",             0, "17:30" },
    { "Sure, what do we need?",                 1, "17:35" },
    { "Milk, eggs, bread, and coffee",          0, "17:36" },
    { "Got it. Anything else?",                 1, "17:38" },
    { "That's all, thanks!",                    0, "17:39" },
    { "No problem",                             1, "17:40" },
    { "Actually, grab some fruit too",          0, "17:42" },
    { "Apples and bananas?",                    1, "17:43" },
};

static const cellphone_sms_thread_t s_sms_threads[CELLPHONE_SMS_THREAD_COUNT] = {
    {
        "Ashley Johnson", "Got it, thanks!",              "Today",
        s_sms_ashley, sizeof(s_sms_ashley) / sizeof(s_sms_ashley[0])
    },
    {
        "Brian Smith",    "10 AM then",                   "Today",
        s_sms_brian,  sizeof(s_sms_brian)  / sizeof(s_sms_brian[0])
    },
    {
        "Chris Williams", "Sounds good, how about Friday?", "Yesterday",
        s_sms_chris,  sizeof(s_sms_chris)  / sizeof(s_sms_chris[0])
    },
    {
        "Daniel Brown",   "Apples and bananas?",          "Yesterday",
        s_sms_daniel, sizeof(s_sms_daniel) / sizeof(s_sms_daniel[0])
    },
};

const cellphone_sms_thread_t * cellphone_data_sms_threads(void)
{
    return s_sms_threads;
}

/*********************
 * CALL LOG
 *********************/
static const cellphone_calllog_entry_t s_calllog[CELLPHONE_CALLLOG_COUNT] = {
    { "Ashley Johnson", "+1-555-0101", "Today, 14:22",      CELLPHONE_CALL_INCOMING },
    { "Brian Smith",    "+1-555-0102", "Today, 13:05",      CELLPHONE_CALL_OUTGOING },
    { "Unknown",        "+1-555-9999", "Today, 11:30",      CELLPHONE_CALL_MISSED },
    { "Chris Williams", "+1-555-0103", "Today, 10:15",      CELLPHONE_CALL_INCOMING },
    { "Daniel Brown",   "+1-555-0104", "Today, 09:45",      CELLPHONE_CALL_OUTGOING },
    { "Emily Davis",    "+1-555-0105", "Yesterday, 18:30",  CELLPHONE_CALL_MISSED },
    { "Frank Miller",   "+1-555-0106", "Yesterday, 16:20",  CELLPHONE_CALL_INCOMING },
    { "Grace Wilson",   "+1-555-0107", "Yesterday, 14:10",  CELLPHONE_CALL_OUTGOING },
    { "Ashley Johnson", "+1-555-0101", "Yesterday, 12:00",  CELLPHONE_CALL_INCOMING },
    { "Henry Thomas",   "+1-555-0108", "Yesterday, 10:30",  CELLPHONE_CALL_MISSED },
    { "Isaac Moore",    "+1-555-0109", "Apr 23, 17:45",     CELLPHONE_CALL_OUTGOING },
    { "James Taylor",   "+1-555-0110", "Apr 23, 15:00",     CELLPHONE_CALL_INCOMING },
    { "Karen Harris",   "+1-555-0111", "Apr 22, 09:30",     CELLPHONE_CALL_MISSED },
    { "Laura Jackson",  "+1-555-0112", "Apr 22, 08:15",     CELLPHONE_CALL_OUTGOING },
    { "Brian Smith",    "+1-555-0102", "Apr 21, 20:00",     CELLPHONE_CALL_INCOMING },
};

const cellphone_calllog_entry_t * cellphone_data_calllog(void)
{
    return s_calllog;
}

/*********************
 * MUSIC
 *********************/
static const cellphone_track_t s_tracks[CELLPHONE_TRACK_COUNT] = {
    { "Summer in the City",       "The Highway Drifters", "Open Road",   214 },
    { "Heart on the Run",         "Ocean Avenue",         "Coastline",   187 },
    { "Tomorrow's Light",         "Neon Skyline",         "City Lights", 245 },
    { "Coming Home",              "The Highway Drifters", "Open Road",   198 },
    { "Out of the Blue",          "Ocean Avenue",         "Coastline",   220 },
    { "Last Train Out",           "Neon Skyline",         "City Lights", 176 },
    { "Catch the Wind",           "Neon Skyline",         "City Lights", 203 },
    { "Roadside Diner",           "The Highway Drifters", "Open Road",   262 },
    { "Letters Unsent",           "Ocean Avenue",         "Coastline",   195 },
    { "Goodbye for Now",          "Neon Skyline",         "City Lights", 240 },
};

const cellphone_track_t * cellphone_data_tracks(void)
{
    return s_tracks;
}

#endif /* LV_USE_DEMO_CELLPHONE */
