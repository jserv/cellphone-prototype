/**
 * @file lv_demo_cellphone_skin.c
 *
 * Device skin rendering and management for the Cell Phone Demo.
 * Draws a phone body frame around the LCD content area.
 *
 * Default skin: procedurally drawn dark bezel with speaker grille
 * and home button indicator. No image assets required.
 */

#include "lv_demo_cellphone_skin.h"
#include "lv_demo_cellphone_lock.h"

#if LV_USE_DEMO_CELLPHONE && defined(LV_DEMO_CELLPHONE_SKIN) && LV_DEMO_CELLPHONE_SKIN && LV_USE_SDL

/*********************
 *      DEFINES
 *********************/

/* Default procedural skin geometry for 240x320 LCD */
#define SKIN_DEFAULT_BEZEL_LR   30   /* left/right bezel width  */
#define SKIN_DEFAULT_BEZEL_TOP  60   /* top bezel (speaker area) */
#define SKIN_DEFAULT_BEZEL_BOT  50   /* bottom bezel (home btn)  */

#define SKIN_DEFAULT_FRAME_W    (CELLPHONE_HOR_RES + 2 * SKIN_DEFAULT_BEZEL_LR)
#define SKIN_DEFAULT_FRAME_H    (CELLPHONE_VER_RES + SKIN_DEFAULT_BEZEL_TOP + SKIN_DEFAULT_BEZEL_BOT)
#define SKIN_DEFAULT_LCD_X      SKIN_DEFAULT_BEZEL_LR
#define SKIN_DEFAULT_LCD_Y      SKIN_DEFAULT_BEZEL_TOP

/* "Bare" skin: no bezel, LCD fills window */
#define SKIN_BARE_FRAME_W       CELLPHONE_HOR_RES
#define SKIN_BARE_FRAME_H       CELLPHONE_VER_RES

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void draw_procedural_bezel(lv_obj_t * parent, const cellphone_skin_t * skin);
static void reset_bezel_obj(lv_obj_t * bezel, const cellphone_skin_t * skin);
static void populate_bezel(lv_obj_t * bezel, const cellphone_skin_t * skin);
static void create_hotspot_regions(lv_obj_t * parent, const cellphone_skin_t * skin);
static void hotspot_press_cb(lv_event_t * e);

/**********************
 *  STATIC VARIABLES
 **********************/

/*
 * Hotspot arrays for procedural skins.
 * Coordinates in skin-image space, computed from bezel geometry.
 *
 * Home button: centered circle in bottom bezel.
 * Power button: small region on right edge near top of LCD.
 */
#define HS_HOME_SIZE  34  /* generous click zone around the 28px circle */
#define HS_HOME_X1    (SKIN_DEFAULT_LCD_X + (CELLPHONE_HOR_RES - HS_HOME_SIZE) / 2)
#define HS_HOME_Y1    (SKIN_DEFAULT_LCD_Y + CELLPHONE_VER_RES + \
                       (SKIN_DEFAULT_BEZEL_BOT - HS_HOME_SIZE) / 2)
#define HS_PWR_W      12
#define HS_PWR_H      30
#define HS_PWR_X1     (SKIN_DEFAULT_FRAME_W - HS_PWR_W)
#define HS_PWR_Y1     (SKIN_DEFAULT_LCD_Y + 16)

static const cellphone_hotspot_t s_hotspots_default[] = {
    {
        HS_HOME_X1, HS_HOME_Y1,
        HS_HOME_X1 + HS_HOME_SIZE, HS_HOME_Y1 + HS_HOME_SIZE,
        CELLPHONE_KEY_HOME
    },
    {
        HS_PWR_X1, HS_PWR_Y1,
        HS_PWR_X1 + HS_PWR_W, HS_PWR_Y1 + HS_PWR_H,
        CELLPHONE_KEY_POWER
    },
};

#define HOTSPOT_DEFAULT_COUNT \
    (sizeof(s_hotspots_default) / sizeof(s_hotspots_default[0]))

/* Built-in skin definitions */
static const cellphone_skin_t s_skins[] = {
    {
        .name       = "Classic Dark",
        .frame_img  = NULL,
        .lcd_x      = SKIN_DEFAULT_LCD_X,
        .lcd_y      = SKIN_DEFAULT_LCD_Y,
        .lcd_w      = CELLPHONE_HOR_RES,
        .lcd_h      = CELLPHONE_VER_RES,
        .frame_w    = SKIN_DEFAULT_FRAME_W,
        .frame_h    = SKIN_DEFAULT_FRAME_H,
        .bg_color   = LV_COLOR_MAKE(0x1a, 0x1a, 0x1a),
        .bezel_color = LV_COLOR_MAKE(0x33, 0x33, 0x33),
        .hotspots   = s_hotspots_default,
        .hotspot_count = HOTSPOT_DEFAULT_COUNT,
    },
    {
        .name       = "Titanium",
        .frame_img  = NULL,
        .lcd_x      = SKIN_DEFAULT_LCD_X,
        .lcd_y      = SKIN_DEFAULT_LCD_Y,
        .lcd_w      = CELLPHONE_HOR_RES,
        .lcd_h      = CELLPHONE_VER_RES,
        .frame_w    = SKIN_DEFAULT_FRAME_W,
        .frame_h    = SKIN_DEFAULT_FRAME_H,
        .bg_color   = LV_COLOR_MAKE(0xCE, 0xD0, 0xD4),
        .bezel_color = LV_COLOR_MAKE(0x78, 0x80, 0x88),
        .hotspots   = s_hotspots_default,
        .hotspot_count = HOTSPOT_DEFAULT_COUNT,
    },
    {
        .name       = "None",
        .frame_img  = NULL,
        .lcd_x      = 0,
        .lcd_y      = 0,
        .lcd_w      = CELLPHONE_HOR_RES,
        .lcd_h      = CELLPHONE_VER_RES,
        .frame_w    = SKIN_BARE_FRAME_W,
        .frame_h    = SKIN_BARE_FRAME_H,
        .bg_color   = LV_COLOR_MAKE(0x00, 0x00, 0x00),
        .bezel_color = LV_COLOR_MAKE(0x00, 0x00, 0x00),
        .hotspots   = NULL,
        .hotspot_count = 0,
    },
};

#define SKIN_COUNT (sizeof(s_skins) / sizeof(s_skins[0]))

/*
 * Tint presets: simulate display characteristics.
 * PicoGUI tint values: NiftyCOMM=#D7E5D4, VR3=#687266.
 */
static const cellphone_tint_preset_t s_tint_presets[] = {
    { "None",        {0},                        LV_OPA_TRANSP },
    { "Green Mono",  LV_COLOR_MAKE(0xD7, 0xE5, 0xD4), LV_OPA_30 },
    { "Warm OLED",   LV_COLOR_MAKE(0xFF, 0xE0, 0xB0), LV_OPA_10 },
    { "Cool TN",     LV_COLOR_MAKE(0xB0, 0xC0, 0xD8), LV_OPA_20 },
};

#define TINT_PRESET_COUNT (sizeof(s_tint_presets) / sizeof(s_tint_presets[0]))

static uint32_t   s_active_idx = 0;
static lv_obj_t * s_skin_parent = NULL;
static lv_obj_t * s_bezel_obj   = NULL;  /* the drawn bezel container */
static lv_obj_t * s_lcd_cont    = NULL;  /* LCD content container     */
static lv_obj_t * s_tint_overlay = NULL; /* semi-transparent tint layer*/
static uint32_t   s_tint_idx = 0;      /* active tint preset index   */
static lv_display_t * s_skin_disp = NULL;

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

uint32_t cellphone_skin_count(void)
{
    return SKIN_COUNT;
}

const cellphone_skin_t * cellphone_skin_get(uint32_t idx)
{
    if(idx >= SKIN_COUNT) return NULL;
    return &s_skins[idx];
}

uint32_t cellphone_skin_active_index(void)
{
    return s_active_idx;
}

const cellphone_skin_t * cellphone_skin_active(void)
{
    return &s_skins[s_active_idx];
}

void cellphone_skin_get_window_size(int32_t * w, int32_t * h)
{
    const cellphone_skin_t * skin = cellphone_skin_active();
    if(w) *w = skin->frame_w;
    if(h) *h = skin->frame_h;
}

lv_obj_t * cellphone_skin_create(lv_obj_t * parent)
{
    const cellphone_skin_t * skin = cellphone_skin_active();
    s_skin_parent = parent;
    s_skin_disp = lv_obj_get_display(parent);

    /* Bezel layer: sits behind LCD content */
    s_bezel_obj = lv_obj_create(parent);
    reset_bezel_obj(s_bezel_obj, skin);
    populate_bezel(s_bezel_obj, skin);

    /* LCD container: positioned at the LCD offset within the frame */
    s_lcd_cont = cellphone_obj_bare(parent);
    lv_obj_set_pos(s_lcd_cont, skin->lcd_x, skin->lcd_y);
    lv_obj_set_size(s_lcd_cont, skin->lcd_w, skin->lcd_h);
    lv_obj_clear_flag(s_lcd_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(s_lcd_cont, CELLPHONE_COLOR_BG, 0);
    lv_obj_set_style_bg_opa(s_lcd_cont, LV_OPA_COVER, 0);

    /* Tint overlay: semi-transparent layer on top of LCD for
     * display color simulation. Starts transparent (no tint). */
    s_tint_overlay = cellphone_obj_bare(parent);
    lv_obj_set_pos(s_tint_overlay, skin->lcd_x, skin->lcd_y);
    lv_obj_set_size(s_tint_overlay, skin->lcd_w, skin->lcd_h);
    lv_obj_set_style_bg_opa(s_tint_overlay, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(s_tint_overlay, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

    return s_lcd_cont;
}

void cellphone_skin_set(uint32_t idx)
{
    if(idx >= SKIN_COUNT) return;
    if(idx == s_active_idx) return;

    s_active_idx = idx;
    const cellphone_skin_t * skin = &s_skins[idx];

    /* Validate cached pointers -- demo may have been torn down */
    if(s_bezel_obj && !lv_obj_is_valid(s_bezel_obj)) s_bezel_obj = NULL;
    if(s_lcd_cont && !lv_obj_is_valid(s_lcd_cont)) s_lcd_cont = NULL;
    if(s_tint_overlay && !lv_obj_is_valid(s_tint_overlay)) s_tint_overlay = NULL;
    if(s_skin_parent && !lv_obj_is_valid(s_skin_parent)) s_skin_parent = NULL;

    /* Redraw bezel */
    if(s_bezel_obj) {
        lv_obj_clean(s_bezel_obj);
        reset_bezel_obj(s_bezel_obj, skin);
        populate_bezel(s_bezel_obj, skin);
    }

    /* Reposition LCD container and tint overlay */
    if(s_lcd_cont) {
        lv_obj_set_pos(s_lcd_cont, skin->lcd_x, skin->lcd_y);
        lv_obj_set_size(s_lcd_cont, skin->lcd_w, skin->lcd_h);
    }
    if(s_tint_overlay) {
        lv_obj_set_pos(s_tint_overlay, skin->lcd_x, skin->lcd_y);
        lv_obj_set_size(s_tint_overlay, skin->lcd_w, skin->lcd_h);
    }

    /* Resize the SDL window to match the new skin.
     * lv_display_set_resolution triggers a window resize in the SDL driver. */
    lv_display_t * disp = s_skin_disp;
    if(!disp && s_skin_parent) {
        disp = lv_obj_get_display(s_skin_parent);
    }
    if(disp && s_skin_parent && lv_obj_get_parent(s_skin_parent) == lv_display_get_screen_active(disp)) {
        lv_display_set_resolution(disp, skin->frame_w, skin->frame_h);
    }

    /* Resize the parent to match */
    if(s_skin_parent) {
        lv_obj_set_size(s_skin_parent, skin->frame_w, skin->frame_h);
    }
}

void cellphone_skin_reset(void)
{
    s_skin_parent  = NULL;
    s_bezel_obj    = NULL;
    s_lcd_cont     = NULL;
    s_tint_overlay = NULL;
    s_skin_disp    = NULL;
}

uint32_t cellphone_tint_preset_count(void)
{
    return TINT_PRESET_COUNT;
}

const cellphone_tint_preset_t * cellphone_tint_preset_get(uint32_t idx)
{
    if(idx >= TINT_PRESET_COUNT) return NULL;
    return &s_tint_presets[idx];
}

void cellphone_skin_set_tint(lv_color_t color, lv_opa_t opa)
{
    if(!s_tint_overlay) return;
    if(!lv_obj_is_valid(s_tint_overlay)) {
        s_tint_overlay = NULL;
        return;
    }
    lv_obj_set_style_bg_color(s_tint_overlay, color, 0);
    lv_obj_set_style_bg_opa(s_tint_overlay, opa, 0);
}

void cellphone_skin_set_tint_preset(uint32_t idx)
{
    if(idx >= TINT_PRESET_COUNT) return;
    s_tint_idx = idx;
    const cellphone_tint_preset_t * p = &s_tint_presets[idx];
    cellphone_skin_set_tint(p->color, p->opa);
}

uint32_t cellphone_tint_active_index(void)
{
    return s_tint_idx;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/**
 * Draw a procedural phone bezel using LVGL objects.
 * Creates a dark rounded body with speaker grille and home button indicator.
 */
static void reset_bezel_obj(lv_obj_t * bezel, const cellphone_skin_t * skin)
{
    lv_obj_remove_style_all(bezel);
    lv_obj_set_pos(bezel, 0, 0);
    lv_obj_set_size(bezel, skin->frame_w, skin->frame_h);
    lv_obj_clear_flag(bezel, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
}

/**
 * Set up the bezel with image or procedural drawing plus hotspots.
 * Shared by cellphone_skin_create() and cellphone_skin_set().
 */
static void populate_bezel(lv_obj_t * bezel, const cellphone_skin_t * skin)
{
    if(skin->frame_img) {
        lv_obj_t * img = lv_image_create(bezel);
        lv_image_set_src(img, skin->frame_img);
        lv_obj_set_pos(img, 0, 0);
    }
    else if(skin->lcd_x > 0 || skin->lcd_y > 0) {
        draw_procedural_bezel(bezel, skin);
    }
    create_hotspot_regions(bezel, skin);
}

static void draw_procedural_bezel(lv_obj_t * parent, const cellphone_skin_t * skin)
{
    /* Phone body: rounded dark rectangle filling the frame */
    lv_obj_set_style_bg_color(parent, skin->bg_color, 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(parent, 20, 0);
    lv_obj_set_style_border_color(parent, skin->bezel_color, 0);
    lv_obj_set_style_border_width(parent, 2, 0);
    lv_obj_set_style_border_opa(parent, LV_OPA_COVER, 0);

    /* Speaker grille: small rounded rectangle centered in the top bezel */
    lv_obj_t * speaker = cellphone_obj_bare(parent);
    int16_t spk_w = skin->lcd_w / 3;
    int16_t spk_h = 4;
    int16_t spk_x = skin->lcd_x + (skin->lcd_w - spk_w) / 2;
    int16_t spk_y = skin->lcd_y / 2 - spk_h / 2;
    lv_obj_set_pos(speaker, spk_x, spk_y);
    lv_obj_set_size(speaker, spk_w, spk_h);
    lv_obj_set_style_bg_color(speaker, skin->bezel_color, 0);
    lv_obj_set_style_bg_opa(speaker, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(speaker, 2, 0);
    lv_obj_clear_flag(speaker, LV_OBJ_FLAG_CLICKABLE);

    /* Camera dot: small circle to the right of speaker */
    lv_obj_t * camera = cellphone_obj_bare(parent);
    int16_t cam_size = 6;
    int16_t cam_x = spk_x + spk_w + 12;
    int16_t cam_y = spk_y + (spk_h - cam_size) / 2;
    lv_obj_set_pos(camera, cam_x, cam_y);
    lv_obj_set_size(camera, cam_size, cam_size);
    lv_obj_set_style_bg_color(camera, skin->bezel_color, 0);
    lv_obj_set_style_bg_opa(camera, LV_OPA_80, 0);
    lv_obj_set_style_radius(camera, LV_RADIUS_CIRCLE, 0);
    lv_obj_clear_flag(camera, LV_OBJ_FLAG_CLICKABLE);

    /* LCD bezel inset: thin dark border around the LCD cutout */
    lv_obj_t * lcd_border = cellphone_obj_bare(parent);
    lv_obj_set_pos(lcd_border, skin->lcd_x - 2, skin->lcd_y - 2);
    lv_obj_set_size(lcd_border, skin->lcd_w + 4, skin->lcd_h + 4);
    lv_obj_set_style_bg_opa(lcd_border, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(lcd_border, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_width(lcd_border, 1, 0);
    lv_obj_set_style_border_opa(lcd_border, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(lcd_border, 1, 0);
    lv_obj_clear_flag(lcd_border, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

    /* Home button indicator: circle centered in the bottom bezel */
    int16_t bot_bezel = skin->frame_h - skin->lcd_y - skin->lcd_h;
    if(bot_bezel > 20) {
        lv_obj_t * home_btn = cellphone_obj_bare(parent);
        int16_t btn_size = LV_MIN(bot_bezel - 16, 28);
        int16_t btn_x = skin->lcd_x + (skin->lcd_w - btn_size) / 2;
        int16_t btn_y = skin->lcd_y + skin->lcd_h + (bot_bezel - btn_size) / 2;
        lv_obj_set_pos(home_btn, btn_x, btn_y);
        lv_obj_set_size(home_btn, btn_size, btn_size);
        lv_obj_set_style_bg_opa(home_btn, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_color(home_btn, skin->bezel_color, 0);
        lv_obj_set_style_border_width(home_btn, 2, 0);
        lv_obj_set_style_border_opa(home_btn, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(home_btn, LV_RADIUS_CIRCLE, 0);
        lv_obj_clear_flag(home_btn, LV_OBJ_FLAG_CLICKABLE);
    }

    /* Power button indicator: small rounded rectangle on right edge */
    lv_obj_t * pwr = cellphone_obj_bare(parent);
    lv_obj_set_pos(pwr, skin->frame_w - 4, skin->lcd_y + 24);
    lv_obj_set_size(pwr, 3, 22);
    lv_obj_set_style_bg_color(pwr, skin->bezel_color, 0);
    lv_obj_set_style_bg_opa(pwr, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(pwr, 1, 0);
    lv_obj_clear_flag(pwr, LV_OBJ_FLAG_CLICKABLE);
}

/**
 * Create invisible click regions for hotspot buttons.
 * Each region is an lv_obj that covers the hotspot rectangle
 * and fires a callback on press.
 */
static void create_hotspot_regions(lv_obj_t * parent, const cellphone_skin_t * skin)
{
    if(!skin->hotspots || skin->hotspot_count == 0) return;

    /* The bezel obj needs to be clickable for child hit-testing */
    lv_obj_add_flag(parent, LV_OBJ_FLAG_CLICKABLE);

    for(uint16_t i = 0; i < skin->hotspot_count; i++) {
        const cellphone_hotspot_t * hs = &skin->hotspots[i];

        /* Skip malformed entries */
        if(hs->x1 >= hs->x2 || hs->y1 >= hs->y2) continue;

        lv_obj_t * region = cellphone_obj_bare(parent);
        lv_obj_set_pos(region, hs->x1, hs->y1);
        lv_obj_set_size(region, hs->x2 - hs->x1, hs->y2 - hs->y1);
        lv_obj_set_style_bg_opa(region, LV_OPA_TRANSP, 0);
        lv_obj_add_flag(region, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(region, LV_OBJ_FLAG_SCROLLABLE);
        /* Pass pointer to the static hotspot entry -- stable storage */
        lv_obj_add_event_cb(region, hotspot_press_cb, LV_EVENT_CLICKED,
                            (void *)hs);
    }
}

/**
 * Handle hotspot button press.
 * user_data points to the static cellphone_hotspot_t entry.
 */
static void hotspot_press_cb(lv_event_t * e)
{
    const cellphone_hotspot_t * hs = lv_event_get_user_data(e);
    if(!hs) return;
    bool top_is_lock = cellphone_screen_top_is(cellphone_lock_create);

    switch(hs->key) {
        case CELLPHONE_KEY_HOME:
            if(!top_is_lock) {
                cellphone_screen_home();
            }
            break;
        case CELLPHONE_KEY_POWER:
            if(cellphone_screen_depth() > 0 && !top_is_lock) {
                cellphone_chrome_set_visible(false);
                cellphone_screen_push(cellphone_lock_create);
            }
            break;
        default:
            break;
    }
}

#endif /* LV_USE_DEMO_CELLPHONE && LV_DEMO_CELLPHONE_SKIN && LV_USE_SDL */
