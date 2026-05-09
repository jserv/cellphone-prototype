/**
 * @file lv_demo_cellphone_calc.c
 *
 * Calculator app: four-function arithmetic with percent and sign toggle.
 * Uses scaled integer arithmetic (10^SCALE_DIGITS) to avoid float dependency.
 */

#include "lv_demo_cellphone_calc.h"

#if LV_USE_DEMO_CELLPHONE

/*********************
 *      DEFINES
 *********************/
#define DISPLAY_BUF_LEN   24
#define DISPLAY_H         40
#define SCALE_DIGITS      6            /* decimal digits of precision */
#define SCALE             1000000LL    /* 10^SCALE_DIGITS */

#define CELLPHONE_CALC_KEY_LIST(X) \
    X("C",             0, 0, 1) \
    X("+/-",           1, 0, 1) \
    X("%",             2, 0, 1) \
    X("/",             3, 0, 1) \
    X("7",             0, 1, 1) \
    X("8",             1, 1, 1) \
    X("9",             2, 1, 1) \
    X(LV_SYMBOL_CLOSE, 3, 1, 1) \
    X("4",             0, 2, 1) \
    X("5",             1, 2, 1) \
    X("6",             2, 2, 1) \
    X("-",             3, 2, 1) \
    X("1",             0, 3, 1) \
    X("2",             1, 3, 1) \
    X("3",             2, 3, 1) \
    X("+",             3, 3, 1) \
    X("0",             0, 4, 2) \
    X(".",             2, 4, 1) \
    X("=",             3, 4, 1)

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void calc_key_event_cb(lv_event_t * e);
static void update_display(void);
static void reset_state(void);
static bool execute_op(void);
static bool scaled_mul(int64_t a, int64_t b, int64_t * res);
static void format_scaled(int64_t val);
static void append_digit(char digit);
static void append_dot(void);
static lv_obj_t * calc_key_create(lv_obj_t * parent, const char * text, int32_t col, int32_t row, int32_t col_span);

/**********************
 *  STATIC VARIABLES
 **********************/
static lv_obj_t * s_display;

static char    s_display_buf[DISPLAY_BUF_LEN];
static int64_t s_accumulator;    /* left operand, scaled by SCALE */
static int64_t s_operand;        /* current display value, scaled by SCALE */
static char    s_pending_op;     /* '+', '-', '*', '/', or '\0' */
static bool    s_new_input;      /* next digit replaces display */
static int32_t s_frac_divisor;   /* 0 = integer part; 10,100,... after dot */
static bool    s_negative;       /* sign of operand being entered */
static bool    s_error;          /* division by zero */

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

lv_obj_t * cellphone_calc_create(lv_obj_t * parent)
{
    reset_state();

    int32_t w = CELLPHONE_CONTENT_W;
    int32_t h = CELLPHONE_CONTENT_H;

    /* ---- display label ---- */
    s_display = cellphone_label(parent, "", CELLPHONE_FONT_LARGE, CELLPHONE_COLOR_TEXT);
    lv_obj_set_width(s_display, w - 8);
    lv_obj_set_height(s_display, DISPLAY_H);
    lv_label_set_long_mode(s_display, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_align(s_display, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_pad_right(s_display, 8, 0);
    lv_obj_set_style_pad_top(s_display, 4, 0);
    lv_obj_align(s_display, LV_ALIGN_TOP_RIGHT, -4, 0);
    lv_label_set_text_static(s_display, s_display_buf);

    /* ---- keypad ---- */
    static const int32_t col_dsc[] = {
        LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST
    };
    static const int32_t row_dsc[] = {
        LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST
    };

    lv_obj_t * keypad = cellphone_obj_fill(parent, CELLPHONE_COLOR_CARD);
    lv_obj_set_size(keypad, w, h - DISPLAY_H);
    lv_obj_align(keypad, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_layout(keypad, LV_LAYOUT_GRID);
    lv_obj_set_grid_dsc_array(keypad, col_dsc, row_dsc);
    lv_obj_set_style_border_width(keypad, 0, 0);
    lv_obj_set_style_radius(keypad, 0, 0);
    lv_obj_set_style_pad_all(keypad, 2, 0);
    lv_obj_set_style_pad_row(keypad, 2, 0);
    lv_obj_set_style_pad_column(keypad, 2, 0);
    lv_obj_clear_flag(keypad, LV_OBJ_FLAG_SCROLLABLE);

#define X(text, col, row, col_span) calc_key_create(keypad, text, col, row, col_span);
    CELLPHONE_CALC_KEY_LIST(X)
#undef X

    return parent;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void reset_state(void)
{
    s_accumulator  = 0;
    s_operand      = 0;
    s_pending_op   = '\0';
    s_new_input    = true;
    s_frac_divisor = 0;
    s_negative     = false;
    s_error        = false;
    lv_snprintf(s_display_buf, DISPLAY_BUF_LEN, "0");
}

/**
 * Format a scaled integer (val / SCALE) into s_display_buf.
 * Strips trailing zeros after the decimal point.
 */
static void format_scaled(int64_t val)
{
    char * p = s_display_buf;
    int64_t abs_val = val < 0 ? -val : val;
    int64_t integer_part = abs_val / SCALE;
    int64_t frac_part = abs_val % SCALE;
    int pos = 0;

    if(val < 0) {
        p[pos++] = '-';
    }

    /* Format integer part using lv_snprintf */
    pos += lv_snprintf(p + pos, DISPLAY_BUF_LEN - pos,
                       "%" LV_PRId64, (int64_t)integer_part);

    /* Format fractional part if nonzero */
    if(frac_part > 0) {
        p[pos++] = '.';
        /* Write SCALE_DIGITS fractional digits */
        int64_t divisor = SCALE / 10;
        int wrote = 0;
        while(divisor > 0 && frac_part > 0 && pos < DISPLAY_BUF_LEN - 1) {
            int64_t d = frac_part / divisor;
            p[pos++] = (char)('0' + d);
            frac_part -= d * divisor;
            divisor /= 10;
            wrote++;
        }
        /* Strip trailing zeros */
        while(wrote > 0 && p[pos - 1] == '0') {
            pos--;
            wrote--;
        }
        /* If dot is last char, remove it too */
        if(p[pos - 1] == '.') {
            pos--;
        }
    }
    p[pos] = '\0';
}

static void update_display(void)
{
    if(s_error) {
        lv_label_set_text(s_display, "Error");
        return;
    }
    lv_label_set_text_static(s_display, s_display_buf);
}

static bool execute_op(void)
{
    switch(s_pending_op) {
        case '+':
            s_accumulator += s_operand;
            break;
        case '-':
            s_accumulator -= s_operand;
            break;
        case '*':
            if(!scaled_mul(s_accumulator, s_operand, &s_accumulator)) {
                s_error = true;
                return false;
            }
            break;
        case '/':
            if(s_operand == 0) {
                s_error = true;
                return false;
            }
            /* Overflow-safe: check before multiply */
            if(s_accumulator > INT64_MAX / SCALE ||
               s_accumulator < INT64_MIN / SCALE) {
                s_error = true;
                return false;
            }
            s_accumulator = (s_accumulator * SCALE) / s_operand;
            break;
        default:
            s_accumulator = s_operand;
            break;
    }
    return true;
}

static bool scaled_mul(int64_t a, int64_t b, int64_t * res)
{
    int64_t a_int = a / SCALE;
    int64_t b_int = b / SCALE;
    int64_t a_frac = a % SCALE;
    int64_t b_frac = b % SCALE;

    /* (a_int*SCALE + a_frac) * (b_int*SCALE + b_frac) / SCALE */
    if(a_int != 0 && (b_int > INT64_MAX / a_int || b_int < INT64_MIN / a_int)) {
        return false;
    }
    int64_t term0 = a_int * b_int;
    if(term0 > INT64_MAX / SCALE || term0 < INT64_MIN / SCALE) {
        return false;
    }
    term0 *= SCALE;

    if(a_int != 0 && (b_frac > INT64_MAX / a_int || b_frac < INT64_MIN / a_int)) {
        return false;
    }
    int64_t term1 = a_int * b_frac;

    if(b_int != 0 && (a_frac > INT64_MAX / b_int || a_frac < INT64_MIN / b_int)) {
        return false;
    }
    int64_t term2 = b_int * a_frac;

    if(a_frac != 0 && (b_frac > INT64_MAX / a_frac || b_frac < INT64_MIN / a_frac)) {
        return false;
    }
    int64_t term3 = (a_frac * b_frac) / SCALE;

    if((term1 > 0 && term0 > INT64_MAX - term1) ||
       (term1 < 0 && term0 < INT64_MIN - term1)) {
        return false;
    }
    term0 += term1;

    if((term2 > 0 && term0 > INT64_MAX - term2) ||
       (term2 < 0 && term0 < INT64_MIN - term2)) {
        return false;
    }
    term0 += term2;

    if((term3 > 0 && term0 > INT64_MAX - term3) ||
       (term3 < 0 && term0 < INT64_MIN - term3)) {
        return false;
    }
    term0 += term3;

    *res = term0;
    return true;
}

static void append_digit(char digit)
{
    if(s_new_input) {
        s_display_buf[0] = digit;
        s_display_buf[1] = '\0';
        s_new_input = false;
        s_frac_divisor = 0;
        s_negative = false;
        s_operand = 0;
    }
    else {
        /* Ignore fractional digits beyond SCALE precision */
        if(s_frac_divisor > SCALE) return;

        uint32_t len = (uint32_t)lv_strlen(s_display_buf);
        if(len < DISPLAY_BUF_LEN - 2) {
            s_display_buf[len] = digit;
            s_display_buf[len + 1] = '\0';
        }
    }

    /* Rebuild s_operand from display string */
    int32_t d = digit - '0';
    if(s_frac_divisor > 0) {
        int64_t frac_val = (int64_t)d * (SCALE / s_frac_divisor);
        if(s_negative) {
            s_operand -= frac_val;
        }
        else {
            s_operand += frac_val;
        }
        s_frac_divisor *= 10;
    }
    else {
        /* Adding integer digit */
        if(s_negative) {
            s_operand = s_operand * 10 - (int64_t)d * SCALE;
        }
        else {
            s_operand = s_operand * 10 + (int64_t)d * SCALE;
        }
    }
}

static void append_dot(void)
{
    if(s_frac_divisor > 0) return;  /* already have a dot */

    s_frac_divisor = 10;  /* next digit is tenths */

    if(s_new_input) {
        lv_snprintf(s_display_buf, DISPLAY_BUF_LEN, "0.");
        s_new_input = false;
        s_operand = 0;
        s_negative = false;
    }
    else {
        uint32_t len = (uint32_t)lv_strlen(s_display_buf);
        if(len < DISPLAY_BUF_LEN - 2) {
            s_display_buf[len] = '.';
            s_display_buf[len + 1] = '\0';
        }
    }
}

static lv_obj_t * calc_key_create(lv_obj_t * parent, const char * text, int32_t col, int32_t row, int32_t col_span)
{
    lv_obj_t * btn = lv_button_create(parent);
    lv_obj_remove_style_all(btn);
    lv_obj_set_grid_cell(btn, LV_GRID_ALIGN_STRETCH, col, col_span,
                         LV_GRID_ALIGN_STRETCH, row, 1);
    lv_obj_set_style_bg_color(btn, CELLPHONE_COLOR_INDICATOR, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(btn, 6, 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_set_style_bg_color(btn, CELLPHONE_COLOR_PRIMARY, LV_STATE_PRESSED);
    lv_obj_add_event_cb(btn, calc_key_event_cb, LV_EVENT_CLICKED, (void *)text);

    lv_obj_t * lbl = cellphone_label(btn, text, CELLPHONE_FONT_NORMAL, CELLPHONE_COLOR_TEXT);
    lv_obj_center(lbl);
    lv_obj_set_style_text_color(lbl, lv_color_white(), LV_STATE_PRESSED);

    return btn;
}

static void calc_key_event_cb(lv_event_t * e)
{
    const char * txt = lv_event_get_user_data(e);
    if(txt == NULL) return;

    /* ---- Clear ---- */
    if(lv_strcmp(txt, "C") == 0) {
        reset_state();
        update_display();
        return;
    }

    /* In error state, only C works */
    if(s_error) return;

    /* ---- Sign toggle ---- */
    if(lv_strcmp(txt, "+/-") == 0) {
        s_operand = -s_operand;
        s_negative = !s_negative;
        format_scaled(s_operand);
        update_display();
        return;
    }

    /* ---- Percent ---- */
    if(lv_strcmp(txt, "%") == 0) {
        s_operand /= 100;
        format_scaled(s_operand);
        update_display();
        s_new_input = true;
        s_frac_divisor = 0;
        return;
    }

    /* ---- Digit ---- */
    if(txt[0] >= '0' && txt[0] <= '9') {
        append_digit(txt[0]);
        update_display();
        return;
    }

    /* ---- Decimal point ---- */
    if(txt[0] == '.') {
        append_dot();
        update_display();
        return;
    }

    /* ---- Equals ---- */
    if(lv_strcmp(txt, "=") == 0) {
        if(s_pending_op != '\0') {
            if(!execute_op()) {
                update_display();
                return;
            }
            s_operand = s_accumulator;
            s_pending_op = '\0';
            format_scaled(s_operand);
            update_display();
            s_new_input = true;
            s_frac_divisor = 0;
        }
        return;
    }

    /* ---- Operators: + - * / ---- */
    char op = '\0';
    if(lv_strcmp(txt, "+") == 0)                op = '+';
    else if(lv_strcmp(txt, "-") == 0)           op = '-';
    else if(lv_strcmp(txt, "/") == 0)           op = '/';
    else if(lv_strcmp(txt, LV_SYMBOL_CLOSE) == 0) op = '*';

    if(op != '\0') {
        if(s_pending_op != '\0' && !s_new_input) {
            if(!execute_op()) {
                update_display();
                return;
            }
            s_operand = s_accumulator;
            format_scaled(s_operand);
            update_display();
        }
        else {
            s_accumulator = s_operand;
        }
        s_pending_op = op;
        s_new_input = true;
        s_frac_divisor = 0;
        s_negative = false;
    }
}

#endif /* LV_USE_DEMO_CELLPHONE */
