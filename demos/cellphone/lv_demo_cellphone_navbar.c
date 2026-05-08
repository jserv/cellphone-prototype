/**
 * @file lv_demo_cellphone_navbar.c
 *
 * Persistent bottom navigation bar with three tabs: Dialer, a contextual
 * middle tab, and Contacts.  Each tab is a glyph + label stacked vertically;
 * the tab matching the current top screen gets the "active" highlight.
 *
 * The middle tab is contextual: on the home screen it shows Home (and acts
 * as a no-op anchor); anywhere deeper it shows Back (and pops one level).
 * When in Back mode it carries no highlight, since "Back" is an action,
 * not a destination.
 */

#include "lv_demo_cellphone_navbar.h"
#include "lv_demo_cellphone_dialer.h"
#include "lv_demo_cellphone_contacts.h"

#if LV_USE_DEMO_CELLPHONE

typedef enum {
    NAV_TAB_DIALER = 0,
    NAV_TAB_MENU,
    NAV_TAB_CONTACTS,
    NAV_TAB_COUNT
} navbar_tab_id_t;

typedef struct {
    const char * glyph;
    const char * label;
} navbar_tab_def_t;

/* Glyph picks are constrained to the icon fallback subset wired in
 * lv_demo_cellphone.c. The middle tab is named "Home" (not "Menu") so the
 * HOME glyph and the "go-home" action agree. */
static const navbar_tab_def_t s_tabs[NAV_TAB_COUNT] = {
    [NAV_TAB_DIALER]   = { LV_SYMBOL_CALL, "Dialer"   },
    [NAV_TAB_MENU]     = { LV_SYMBOL_HOME, "Home"     },
    [NAV_TAB_CONTACTS] = { LV_SYMBOL_LIST, "Contacts" },
};

static lv_obj_t * s_buttons[NAV_TAB_COUNT];

/* Index of the glyph and text labels inside each navbar button.  Set by
 * navbar_button_create() (column flex: glyph first, text second). */
#define NAVBAR_BTN_GLYPH_IDX 0
#define NAVBAR_BTN_TEXT_IDX  1

static navbar_tab_id_t resolve_active_tab(void)
{
    if(cellphone_screen_top_is(cellphone_dialer_create))   return NAV_TAB_DIALER;
    if(cellphone_screen_top_is(cellphone_contacts_create)) return NAV_TAB_CONTACTS;
    /* Home screen, plus any other screen, defaults to Menu. */
    return NAV_TAB_MENU;
}

/* Middle tab is contextual: when we're somewhere deeper than the home
 * screen it acts as one-level Back; on home it stays as Menu. The
 * Dialer/Contacts tabs always push their respective screens. */
static bool middle_tab_is_back(void)
{
    return cellphone_screen_depth() > 1;
}

static void navigate_root_tab(cellphone_screen_create_fn fn)
{
    if(!fn) return;
    if(cellphone_screen_top_is(fn)) return;

    /* Persistent tabs are destinations, not history entries. Reset back to
     * Home before opening the requested root screen so tab switches do not
     * accumulate duplicate roots on the stack. */
    cellphone_screen_home();
    cellphone_screen_push(fn);
}

static void tab_clicked_cb(lv_event_t * e)
{
    navbar_tab_id_t id = (navbar_tab_id_t)(intptr_t)lv_event_get_user_data(e);

    switch(id) {
        case NAV_TAB_DIALER:
            navigate_root_tab(cellphone_dialer_create);
            break;
        case NAV_TAB_MENU:
            if(middle_tab_is_back()) {
                cellphone_screen_pop();
            }
            else {
                cellphone_screen_home();  /* no-op when already on home */
            }
            break;
        case NAV_TAB_CONTACTS:
            navigate_root_tab(cellphone_contacts_create);
            break;
        default:
            break;
    }
}

static lv_obj_t * navbar_button_create(lv_obj_t * bar, navbar_tab_id_t id)
{
    /* Bare obj instead of lv_button: the default theme stacks ~10 extra
     * style[] entries per button (pressed/checked/focused parts) that this
     * demo never uses. */
    lv_obj_t * btn = cellphone_obj_bare(bar);
    /* Equal-width tabs (HOR_RES / NAV_TAB_COUNT) so SPACE_EVENLY centers
     * each tab at a predictable x. Without this, asymmetric label widths
     * (e.g. "Dialer" vs "Contacts") shift the Menu tab center off
     * HOR_RES/2, which the test harness assumes is hittable. */
    lv_obj_set_size(btn, CELLPHONE_HOR_RES / NAV_TAB_COUNT, CELLPHONE_NAVBAR_H - 4);
    lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(btn, 2, 0);
    lv_obj_set_style_radius(btn, 4, 0);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);

    cellphone_label(btn, s_tabs[id].glyph, CELLPHONE_FONT_NORMAL, CELLPHONE_COLOR_TEXT);
    cellphone_label(btn, s_tabs[id].label, CELLPHONE_FONT_SM, CELLPHONE_COLOR_TEXT);

    lv_obj_add_event_cb(btn, tab_clicked_cb, LV_EVENT_CLICKED, (void *)(intptr_t)id);

    return btn;
}

/* `active == NAV_TAB_COUNT` means "no destination active" -- used when the
 * middle tab has been swapped to the contextual Back action. Highlighting an
 * action like "Back" as if it were a destination is semantically wrong, so
 * all tabs render unhighlighted in that case. */
static void apply_active_highlight(navbar_tab_id_t active)
{
    lv_color_t hl = lv_color_mix(CELLPHONE_COLOR_PRIMARY,
                                 CELLPHONE_COLOR_NAVBAR, LV_OPA_40);

    for(navbar_tab_id_t i = 0; i < NAV_TAB_COUNT; i++) {
        if(!s_buttons[i]) continue;
        if(i == active) {
            cellphone_obj_paint_fill(s_buttons[i], hl);
        }
        else {
            lv_obj_set_style_bg_opa(s_buttons[i], LV_OPA_TRANSP, 0);
        }
    }
}

static void update_middle_tab_text(void)
{
    /* Resolve the glyph + text labels by child index instead of caching
     * pointers: the cache survives s_buttons[NAV_TAB_MENU] but dangles if
     * any future code rebuilds the button's children (badge, animation,
     * theme switch). */
    lv_obj_t * btn = s_buttons[NAV_TAB_MENU];
    if(!btn || lv_obj_get_child_count(btn) < 2) return;

    lv_obj_t * glyph = lv_obj_get_child(btn, NAVBAR_BTN_GLYPH_IDX);
    lv_obj_t * text  = lv_obj_get_child(btn, NAVBAR_BTN_TEXT_IDX);

    if(middle_tab_is_back()) {
        lv_label_set_text(glyph, LV_SYMBOL_LEFT);
        lv_label_set_text(text,  "Back");
    }
    else {
        lv_label_set_text(glyph, s_tabs[NAV_TAB_MENU].glyph);
        lv_label_set_text(text,  s_tabs[NAV_TAB_MENU].label);
    }
}

void cellphone_navbar_refresh(void)
{
    if(!s_buttons[0]) return;

    navbar_tab_id_t active = resolve_active_tab();
    /* The middle tab is "Back" (an action) when depth > 1.  Highlighting an
     * action as if it were a destination is semantically wrong -- but only
     * suppress when the resolution actually points at Menu.  Dialer/Contacts
     * pages still highlight their own tab to convey "you are here". */
    if(middle_tab_is_back() && active == NAV_TAB_MENU) {
        active = NAV_TAB_COUNT;
    }
    apply_active_highlight(active);
    update_middle_tab_text();
}

bool cellphone_navbar_tab_click(uint32_t index)
{
    if(index >= NAV_TAB_COUNT) return false;
    if(!s_buttons[index]) return false;

    lv_result_t res = lv_obj_send_event(s_buttons[index], LV_EVENT_CLICKED, NULL);
    return res == LV_RESULT_OK;
}

static void navbar_delete_cb(lv_event_t * e)
{
    /* The whole demo root (and thus the navbar's parent layer) is wiped on
     * rebuild via lv_demo_cellphone_with_args. Clear the pointer cache so a
     * subsequent cellphone_navbar_refresh() before recreate doesn't deref
     * freed objects. */
    LV_UNUSED(e);
    for(navbar_tab_id_t i = 0; i < NAV_TAB_COUNT; i++) {
        s_buttons[i] = NULL;
    }
}

void cellphone_navbar_create(lv_obj_t * layer)
{
    /* Vertical-gradient bar: lighter at top, darker at bottom (mirror of
     * the statusbar) so the chrome reads as one piece of glass. */
    lv_obj_t * bar = cellphone_obj_bar(layer, CELLPHONE_COLOR_NAVBAR_GRAD,
                                       CELLPHONE_COLOR_NAVBAR);
    lv_obj_set_pos(bar, 0, CELLPHONE_NAVBAR_Y);
    lv_obj_set_size(bar, CELLPHONE_HOR_RES, CELLPHONE_NAVBAR_H);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_SPACE_EVENLY,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_event_cb(bar, navbar_delete_cb, LV_EVENT_DELETE, NULL);

    for(navbar_tab_id_t i = 0; i < NAV_TAB_COUNT; i++) {
        s_buttons[i] = navbar_button_create(bar, i);
    }

    /* Drive both highlight and middle-tab text through the same code path
     * the runtime uses, so create-time and refresh-time states cannot drift. */
    cellphone_navbar_refresh();
}

#endif /* LV_USE_DEMO_CELLPHONE */
