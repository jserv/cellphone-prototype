/**
 * @file lv_demo_cellphone_settings.c
 *
 * Settings screen using lv_menu with Display, Sound, Wallpaper, About sub-pages.
 */

#include "lv_demo_cellphone_settings.h"
#include "lv_demo_cellphone.h"
#if defined(LV_DEMO_CELLPHONE_SKIN) && LV_DEMO_CELLPHONE_SKIN && LV_USE_SDL
    #include "lv_demo_cellphone_skin.h"
#endif

#if LV_USE_DEMO_CELLPHONE

/**********************
 *  STATIC PROTOTYPES
 **********************/
static lv_obj_t * create_menu_item(lv_obj_t * parent, const char * icon,
                                   const char * text);
static lv_obj_t * create_display_page(lv_obj_t * menu);
static lv_obj_t * create_sound_page(lv_obj_t * menu);
static lv_obj_t * create_wallpaper_page(lv_obj_t * menu);
static lv_obj_t * create_time_page(lv_obj_t * menu);
static lv_obj_t * create_about_page(lv_obj_t * menu);
static void settings_menu_changed_cb(lv_event_t * e);
static void settings_menu_delete_cb(lv_event_t * e);
static lv_obj_t * setting_dropdown(lv_obj_t * page, const char * label,
                                   const char * opts, uint32_t selected,
                                   lv_event_cb_t cb);
static lv_obj_t * page_label(lv_obj_t * parent, const char * text,
                             const lv_font_t * font);
static lv_obj_t * make_time_roller(lv_obj_t * parent, uint32_t mod);
static void style_dropdown_list(lv_obj_t * dd);
static void build_dropdown_opts(char * buf, uint32_t buf_size,
                                const char * (*get_name)(uint32_t idx), uint32_t count);
static const char * theme_name_at(uint32_t idx);
static void theme_dropdown_cb(lv_event_t * e);
#if defined(LV_DEMO_CELLPHONE_SKIN) && LV_DEMO_CELLPHONE_SKIN && LV_USE_SDL
    static const char * skin_name_at(uint32_t idx);
    static const char * tint_name_at(uint32_t idx);
    static void skin_dropdown_cb(lv_event_t * e);
    static void tint_dropdown_cb(lv_event_t * e);
#endif

/**********************
 *  STATIC VARIABLES
 **********************/
static lv_timer_t * s_theme_apply_timer;
static lv_obj_t * s_menu;
static lv_obj_t * s_main_page;
static lv_obj_t * s_display_page;
static lv_obj_t * s_sound_page;
static lv_obj_t * s_wallpaper_page;
static lv_obj_t * s_time_page;
static lv_obj_t * s_about_page;
static uint32_t s_current_page_id;
static uint32_t s_restore_page_id;

#define SETTINGS_PAGE_LIST(X) \
    X(DISPLAY,   LV_SYMBOL_SETTINGS, "Display",   create_display_page) \
    X(SOUND,     LV_SYMBOL_AUDIO,    "Sound",     create_sound_page) \
    X(WALLPAPER, LV_SYMBOL_IMAGE,    "Wallpaper", create_wallpaper_page) \
    X(TIME,      LV_SYMBOL_REFRESH,  "Time",      create_time_page) \
    X(ABOUT,     LV_SYMBOL_LIST,     "About",     create_about_page)

enum {
    SETTINGS_PAGE_ROOT = 0,
#define X(id, icon, title, create_fn) SETTINGS_PAGE_##id,
    SETTINGS_PAGE_LIST(X)
#undef X
};

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

lv_obj_t * cellphone_settings_create(lv_obj_t * parent)
{
    lv_obj_t * menu = lv_menu_create(parent);
    lv_obj_set_size(menu, CELLPHONE_CONTENT_W, CELLPHONE_CONTENT_H);
    lv_obj_set_pos(menu, 0, 0);

    /* Set the font on the menu so all internal widgets (header labels,
     * page titles, back button text) inherit the vec font instead of
     * the zero-glyph LV_FONT_DEFAULT stub. */
    lv_obj_set_style_text_font(menu, CELLPHONE_FONT_NORMAL, 0);

    /* Create sub-pages */
    lv_obj_t * display_page   = create_display_page(menu);
    lv_obj_t * sound_page     = create_sound_page(menu);
    lv_obj_t * wallpaper_page = create_wallpaper_page(menu);
    lv_obj_t * time_page      = create_time_page(menu);
    lv_obj_t * about_page     = create_about_page(menu);

    /* Main page */
    lv_obj_t * main_page = lv_menu_page_create(menu, NULL);

    {
        lv_obj_t * const pages[] = {
            NULL,
            display_page,
            sound_page,
            wallpaper_page,
            time_page,
            about_page,
        };
        lv_obj_t * cont;
        uint32_t page_idx = 1;

#define X(id, icon, title, create_fn) \
    cont = create_menu_item(main_page, icon, title); \
    lv_menu_set_load_page_event(menu, cont, pages[page_idx++]);
        SETTINGS_PAGE_LIST(X)
#undef X
    }

    lv_menu_set_page(menu, main_page);

    s_menu = menu;
    s_main_page = main_page;
    s_display_page = display_page;
    s_sound_page = sound_page;
    s_wallpaper_page = wallpaper_page;
    s_time_page = time_page;
    s_about_page = about_page;
    s_current_page_id = SETTINGS_PAGE_ROOT;

    lv_obj_add_event_cb(menu, settings_menu_changed_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(menu, settings_menu_delete_cb, LV_EVENT_DELETE, NULL);

    if(s_restore_page_id != SETTINGS_PAGE_ROOT) {
        cellphone_settings_refresh_state_restore(s_restore_page_id);
        s_restore_page_id = SETTINGS_PAGE_ROOT;
    }

    return parent;
}

uint32_t cellphone_settings_refresh_state_capture(void)
{
    return s_current_page_id;
}

void cellphone_settings_refresh_state_restore(uint32_t state)
{
    lv_obj_t * page = s_main_page;

    s_restore_page_id = state;
    if(s_menu == NULL) return;

    if(state == SETTINGS_PAGE_DISPLAY) page = s_display_page;
    else if(state == SETTINGS_PAGE_SOUND) page = s_sound_page;
    else if(state == SETTINGS_PAGE_WALLPAPER) page = s_wallpaper_page;
    else if(state == SETTINGS_PAGE_TIME) page = s_time_page;
    else if(state == SETTINGS_PAGE_ABOUT) page = s_about_page;

    if(page) lv_menu_set_page(s_menu, page);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/* Plain (un-colored) label: every settings page wants the menu's inherited
 * text color, so we set only text + font. Centralizing the create+set+font
 * triple removes the rote boilerplate that crowded the page builders. */
static lv_obj_t * page_label(lv_obj_t * parent, const char * text,
                             const lv_font_t * font)
{
    lv_obj_t * lbl = lv_label_create(parent);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_font(lbl, font, 0);
    return lbl;
}

static lv_obj_t * create_menu_item(lv_obj_t * parent, const char * icon,
                                   const char * text)
{
    lv_obj_t * cont = lv_menu_cont_create(parent);
    page_label(cont, icon, CELLPHONE_FONT_NORMAL);
    page_label(cont, text, CELLPHONE_FONT_NORMAL);
    return cont;
}

static lv_obj_t * create_display_page(lv_obj_t * menu)
{
    lv_obj_t * page = lv_menu_page_create(menu, "Display");

    lv_obj_t * cont = lv_menu_cont_create(page);
    page_label(cont, "Brightness", CELLPHONE_FONT_SM);

    lv_obj_t * slider = lv_slider_create(cont);
    lv_slider_set_range(slider, 0, 100);
    lv_slider_set_value(slider, 70, LV_ANIM_OFF);
    lv_obj_set_flex_grow(slider, 1);

    /* Theme selector */
    char buf[128];
    build_dropdown_opts(buf, sizeof(buf), theme_name_at, cellphone_theme_count());
    setting_dropdown(page, "Theme", buf,
                     cellphone_theme_active_index(), theme_dropdown_cb);

#if defined(LV_DEMO_CELLPHONE_SKIN) && LV_DEMO_CELLPHONE_SKIN && LV_USE_SDL
    build_dropdown_opts(buf, sizeof(buf), skin_name_at, cellphone_skin_count());
    setting_dropdown(page, "Device Skin", buf,
                     cellphone_skin_active_index(), skin_dropdown_cb);

    build_dropdown_opts(buf, sizeof(buf), tint_name_at, cellphone_tint_preset_count());
    setting_dropdown(page, "LCD Tint", buf,
                     cellphone_tint_active_index(), tint_dropdown_cb);
#endif

    return page;
}

static lv_obj_t * create_sound_page(lv_obj_t * menu)
{
    lv_obj_t * page = lv_menu_page_create(menu, "Sound");

    lv_obj_t * cont = lv_menu_cont_create(page);
    page_label(cont, "Volume", CELLPHONE_FONT_SM);

    lv_obj_t * slider = lv_slider_create(cont);
    lv_slider_set_range(slider, 0, 100);
    lv_slider_set_value(slider, 50, LV_ANIM_OFF);
    lv_obj_set_flex_grow(slider, 1);

    return page;
}

static lv_obj_t * create_wallpaper_page(lv_obj_t * menu)
{
    lv_obj_t * page = lv_menu_page_create(menu, "Wallpaper");

    lv_obj_t * cont = lv_menu_cont_create(page);
    lv_obj_t * lbl = page_label(cont, "Choose color", CELLPHONE_FONT_SM);
    lv_obj_set_width(lbl, lv_pct(100));

    /* Color swatch buttons: sage / warm tan / slate -- earth tones that fit
     * the retro palette (replaces the previous Material Blue/Green/Purple). */
    static const uint32_t colors[] = { 0x7A9B5E, 0xD0995A, 0x6B8CAE };
    uint32_t i;
    for(i = 0; i < 3; i++) {
        lv_obj_t * btn = lv_button_create(cont);
        lv_obj_set_size(btn, 40, 40);
        lv_obj_set_style_bg_color(btn, lv_color_hex(colors[i]), 0);
        lv_obj_set_style_radius(btn, 8, 0);
    }

    return page;
}

/* Two-digit infinite roller for HH (mod=24) or MM (mod=60). The hour and
 * minute rollers were previously duplicated end-to-end; this helper folds
 * them into one builder driven by the modulus. */
static lv_obj_t * make_time_roller(lv_obj_t * parent, uint32_t mod)
{
    /* Largest case is mm: 60 entries × 3 chars (digit, digit, '\n') = 180. */
    char opts[256];
    uint32_t pos = 0;
    for(uint32_t v = 0; v < mod; v++) {
        if(v > 0) opts[pos++] = '\n';
        opts[pos++] = (char)('0' + v / 10);
        opts[pos++] = (char)('0' + v % 10);
    }
    opts[pos] = '\0';

    lv_obj_t * roller = lv_roller_create(parent);
    lv_roller_set_options(roller, opts, LV_ROLLER_MODE_INFINITE);
    lv_roller_set_visible_row_count(roller, 3);
    lv_obj_set_width(roller, 50);
    lv_obj_set_style_text_font(roller, CELLPHONE_FONT_LARGE, 0);
    lv_obj_set_style_text_font(roller, CELLPHONE_FONT_LARGE, LV_PART_SELECTED);
    return roller;
}

static lv_obj_t * create_time_page(lv_obj_t * menu)
{
    lv_obj_t * page = lv_menu_page_create(menu, "Time");

    lv_obj_t * cont = lv_menu_cont_create(page);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(cont, 8, 0);

    page_label(cont, "Set Time", CELLPHONE_FONT_HEADING);

    /* Hour / Minute roller row */
    lv_obj_t * roller_row = lv_obj_create(cont);
    lv_obj_remove_style_all(roller_row);
    lv_obj_set_size(roller_row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(roller_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(roller_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(roller_row, 8, 0);

    make_time_roller(roller_row, 24);
    page_label(roller_row, ":", CELLPHONE_FONT_LARGE);
    make_time_roller(roller_row, 60);

    /* Note: rollers are display-only for now -- in a real phone they
     * would connect to an RTC driver.  The demo shows the UI pattern. */

    return page;
}

static lv_obj_t * create_about_page(lv_obj_t * menu)
{
    lv_obj_t * page = lv_menu_page_create(menu, "About");
    lv_obj_t * cont = lv_menu_cont_create(page);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(cont, 6, 0);

    page_label(cont, "Cell Phone Demo v1.0",  CELLPHONE_FONT_SM);
    page_label(cont, "LVGL " LVGL_VERSION_INFO, CELLPHONE_FONT_SM);

    char res_text[32];
    lv_snprintf(res_text, sizeof(res_text), "%dx%d", CELLPHONE_HOR_RES, CELLPHONE_VER_RES);
    page_label(cont, res_text, CELLPHONE_FONT_SM);

    return page;
}

static void settings_menu_changed_cb(lv_event_t * e)
{
    lv_obj_t * menu = lv_event_get_target(e);
    lv_obj_t * page = lv_menu_get_cur_main_page(menu);

    struct page_map {
        uint32_t id;
        lv_obj_t * obj;
    };
    const struct page_map pages[] = {
        { SETTINGS_PAGE_DISPLAY, s_display_page },
        { SETTINGS_PAGE_SOUND, s_sound_page },
        { SETTINGS_PAGE_WALLPAPER, s_wallpaper_page },
        { SETTINGS_PAGE_TIME, s_time_page },
        { SETTINGS_PAGE_ABOUT, s_about_page },
    };

    s_current_page_id = SETTINGS_PAGE_ROOT;
    for(uint32_t i = 0; i < sizeof(pages) / sizeof(pages[0]); i++) {
        if(page == pages[i].obj) {
            s_current_page_id = pages[i].id;
            break;
        }
    }
}

static void settings_menu_delete_cb(lv_event_t * e)
{
    if(lv_event_get_target(e) != s_menu) return;

    s_menu = NULL;
    s_main_page = NULL;
    s_display_page = NULL;
    s_sound_page = NULL;
    s_wallpaper_page = NULL;
    s_time_page = NULL;
    s_about_page = NULL;
}

static lv_obj_t * setting_dropdown(lv_obj_t * page, const char * label,
                                   const char * opts, uint32_t selected,
                                   lv_event_cb_t cb)
{
    lv_obj_t * cont = lv_menu_cont_create(page);
    page_label(cont, label, CELLPHONE_FONT_SM);

    lv_obj_t * dd = lv_dropdown_create(cont);
    lv_obj_set_flex_grow(dd, 1);
    lv_obj_set_style_text_font(dd, CELLPHONE_FONT_SM, 0);
    lv_obj_set_style_text_letter_space(dd, 1, 0);
    lv_dropdown_set_options(dd, opts);
    lv_dropdown_set_selected(dd, selected);
    style_dropdown_list(dd);
    lv_obj_add_event_cb(dd, cb, LV_EVENT_VALUE_CHANGED, NULL);

    return dd;
}

static void build_dropdown_opts(char * buf, uint32_t buf_size,
                                const char * (*get_name)(uint32_t idx), uint32_t count)
{
    uint32_t pos = 0;
    for(uint32_t i = 0; i < count && pos < buf_size - 1; i++) {
        if(i > 0 && pos < buf_size - 1) buf[pos++] = '\n';
        const char * name = get_name(i);
        while(*name && pos < buf_size - 1) buf[pos++] = *name++;
    }
    buf[pos] = '\0';
}

static const char * theme_name_at(uint32_t idx)
{
    static const char * const names[] = { "Retro Olive", "Dark" };
    if(idx < sizeof(names) / sizeof(names[0])) return names[idx];
    return "Custom";
}

/**
 * Set the cellphone vec font on a dropdown's popup list so the
 * list items render correctly when LV_FONT_DEFAULT is a stub.
 */
static void style_dropdown_list(lv_obj_t * dd)
{
    lv_obj_t * list = lv_dropdown_get_list(dd);
    if(list == NULL) return;

    const lv_font_t * font = CELLPHONE_FONT_SM;
    lv_obj_t * label = lv_obj_get_child(list, 0);

    /* Set font on the list (LV_PART_MAIN for normal items,
     * LV_PART_SELECTED for the highlighted item overlay). */
    lv_obj_set_style_text_font(list, font, 0);
    lv_obj_set_style_text_font(list, font, LV_PART_SELECTED);
    lv_obj_set_style_text_letter_space(list, 1, 0);
    lv_obj_set_style_text_letter_space(list, 1, LV_PART_SELECTED);

    /* Also set font directly on the label child so it does not
     * fall back to the zero-glyph LV_FONT_DEFAULT stub. */
    if(label) {
        lv_obj_set_style_text_font(label, font, 0);
        lv_obj_set_style_text_letter_space(label, 1, 0);
    }
}

static void theme_apply_timer_cb(lv_timer_t * t)
{
    s_theme_apply_timer = NULL;
    lv_timer_delete(t);
    lv_demo_cellphone_refresh_theme();
}

static void theme_dropdown_cb(lv_event_t * e)
{
    lv_obj_t * dd = lv_event_get_target(e);
    uint32_t sel = lv_dropdown_get_selected(dd);
    if(sel == cellphone_theme_active_index()) return;

    cellphone_theme_set(sel);

    /* Defer the rebuild to the next timer tick so we return from this
     * event callback before the widget tree is destroyed. Coalesce rapid
     * re-selection so we do not queue duplicate zero-delay timers. */
    if(s_theme_apply_timer == NULL) {
        s_theme_apply_timer = lv_timer_create(theme_apply_timer_cb, 0, NULL);
        if(s_theme_apply_timer) lv_timer_set_repeat_count(s_theme_apply_timer, 1);
    }
}

#if defined(LV_DEMO_CELLPHONE_SKIN) && LV_DEMO_CELLPHONE_SKIN && LV_USE_SDL

static const char * skin_name_at(uint32_t idx)
{
    const cellphone_skin_t * s = cellphone_skin_get(idx);
    return s ? s->name : "";
}

static const char * tint_name_at(uint32_t idx)
{
    const cellphone_tint_preset_t * t = cellphone_tint_preset_get(idx);
    return t ? t->name : "";
}

static void skin_dropdown_cb(lv_event_t * e)
{
    lv_obj_t * dd = lv_event_get_target(e);
    uint32_t sel = lv_dropdown_get_selected(dd);
    cellphone_skin_set(sel);
}

static void tint_dropdown_cb(lv_event_t * e)
{
    lv_obj_t * dd = lv_event_get_target(e);
    uint32_t sel = lv_dropdown_get_selected(dd);
    cellphone_skin_set_tint_preset(sel);
}
#endif

#endif /* LV_USE_DEMO_CELLPHONE */
