/**
 * @file lv_demo_cellphone_calllog.c
 *
 * Call history list: scrollable rows showing direction icon, contact name,
 * and timestamp for each logged call.
 */

#include "lv_demo_cellphone_calllog.h"
#include "lv_demo_cellphone_data.h"

#if LV_USE_DEMO_CELLPHONE

/*********************
 *      DEFINES
 *********************/
#define ICON_COL_W      20
#define PAD_ROW         2
#define PAD_INNER       4
#define BORDER_W        1

/**********************
 *  STATIC PROTOTYPES
 **********************/
static lv_obj_t * row_create(lv_obj_t * parent, const cellphone_calllog_entry_t * entry);

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

lv_obj_t * cellphone_calllog_create(lv_obj_t * parent)
{
    const cellphone_calllog_entry_t * log = cellphone_data_calllog();
    uint32_t i;

    /* scrollable container filling the parent content area */
    lv_obj_t * list = cellphone_obj_fill(parent, CELLPHONE_COLOR_BG);
    lv_obj_set_size(list, lv_pct(100), lv_pct(100));
    lv_obj_set_style_pad_all(list, 0, 0);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(list, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(list, 0, 0);
    lv_obj_add_flag(list, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(list, LV_DIR_VER);

    for(i = 0; i < CELLPHONE_CALLLOG_COUNT; i++) {
        row_create(list, &log[i]);
    }

    return parent;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static lv_obj_t * row_create(lv_obj_t * parent, const cellphone_calllog_entry_t * entry)
{
    lv_obj_t * row = cellphone_obj_fill(parent, CELLPHONE_COLOR_CARD);
    lv_obj_set_size(row, lv_pct(100), CELLPHONE_LIST_ROW_H);
    lv_obj_set_style_border_color(row, CELLPHONE_COLOR_INDICATOR, 0);
    lv_obj_set_style_border_width(row, BORDER_W, 0);
    lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_pad_left(row, 6, 0);
    lv_obj_set_style_pad_right(row, 6, 0);
    lv_obj_set_style_pad_top(row, PAD_ROW, 0);
    lv_obj_set_style_pad_bottom(row, PAD_ROW, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, PAD_INNER, 0);
    lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    /* direction icon: missed calls get an X, others get the phone symbol;
     * color encodes incoming / outgoing / missed. */
    const char * sym = entry->dir == CELLPHONE_CALL_MISSED
                       ? LV_SYMBOL_CLOSE : LV_SYMBOL_CALL;
    lv_color_t   sym_color =
        entry->dir == CELLPHONE_CALL_INCOMING ? CELLPHONE_COLOR_CALL_GREEN :
        entry->dir == CELLPHONE_CALL_OUTGOING ? CELLPHONE_COLOR_PRIMARY    :
        entry->dir == CELLPHONE_CALL_MISSED   ? CELLPHONE_COLOR_MISSED_RED :
        CELLPHONE_COLOR_TEXT;

    lv_obj_t * icon = cellphone_label(row, sym, CELLPHONE_FONT_NORMAL, sym_color);
    lv_obj_set_width(icon, ICON_COL_W);
    lv_obj_set_style_text_align(icon, LV_TEXT_ALIGN_CENTER, 0);

    /* column for name + time */
    lv_obj_t * col = cellphone_obj_bare(row);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_grow(col, 1);
    lv_obj_set_height(col, lv_pct(100));
    lv_obj_set_flex_align(col, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(col, 1, 0);
    lv_obj_clear_flag(col, LV_OBJ_FLAG_SCROLLABLE);

    /* contact name */
    lv_obj_t * name_lbl = cellphone_label(col, entry->name,
                                          CELLPHONE_FONT_NORMAL, CELLPHONE_COLOR_TEXT);
    lv_label_set_long_mode(name_lbl, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(name_lbl, lv_pct(100));

    /* time stamp */
    lv_obj_t * time_lbl = cellphone_label(col, entry->time,
                                          CELLPHONE_FONT_SM, CELLPHONE_COLOR_TEXT_SEC);
    lv_label_set_long_mode(time_lbl, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(time_lbl, lv_pct(100));

    return row;
}

#endif /* LV_USE_DEMO_CELLPHONE */
