/**
 * @file lv_demo_cellphone_game.c
 *
 * Lightweight autoplay mini-games.
 */

#include "lv_demo_cellphone_game.h"

#include <stdlib.h>

#if LV_USE_DEMO_CELLPHONE

/*********************
 *      DEFINES
 *********************/
#define ARCADE_BOARD_MARGIN  12
#define ARCADE_STATUS_H      24
#define SNAKE_COLS           14
#define SNAKE_ROWS           12
#define SNAKE_MAX_LEN        96
#define TETRIS_W             10
#define TETRIS_H             16
#define TETRIS_DROP_TICKS    3

/**********************
 *      TYPEDEFS
 **********************/
typedef struct {
    int16_t x;
    int16_t y;
} arcade_pos_t;

typedef enum {
    ARCADE_SNAKE = 0,
    ARCADE_PONG,
    ARCADE_TETRIS,
    ARCADE_KIND_COUNT,
} arcade_kind_t;

typedef struct {
    arcade_pos_t body[SNAKE_MAX_LEN];
    arcade_pos_t fruit;
    arcade_pos_t dir;
    uint16_t len;
    uint16_t resets;
} snake_state_t;

typedef struct {
    int16_t cols;
    int16_t rows;
    int16_t ball_x;
    int16_t ball_y;
    int16_t dir_x;
    int16_t dir_y;
    int16_t paddle_l;
    int16_t paddle_r;
    uint16_t rallies;
} pong_state_t;

typedef struct {
    uint8_t field[TETRIS_H][TETRIS_W];
    uint8_t color[TETRIS_H][TETRIS_W];
    int8_t piece;
    int8_t rotation;
    int8_t piece_x;
    int8_t piece_y;
    int8_t target_x;
    int8_t target_rot;
    uint8_t tick;
    uint16_t lines;
    uint16_t resets;
} tetris_state_t;

/* Per-game vtable.  Indexing one table by arcade_kind_t replaces what
 * used to be four parallel switch statements (create / tick / draw /
 * status), so adding a fourth game is one row, not four call sites. */
typedef struct {
    void (*reset)(void * state);
    void (*update)(void * state);
    void (*draw)(const void * state, lv_layer_t * layer, const lv_area_t * area);
    void (*status)(const void * state, char * buf, size_t size);
    uint32_t period_ms;
} arcade_ops_t;

typedef struct {
    arcade_kind_t kind;
    lv_obj_t * board;
    lv_obj_t * status;
    lv_timer_t * timer;
    union {
        snake_state_t snake;
        pong_state_t pong;
        tetris_state_t tetris;
    } u;
} arcade_game_t;

typedef struct {
    uint32_t magic;
    uint8_t paused;
    uint8_t reserved[3];
    arcade_kind_t kind;
    union {
        snake_state_t snake;
        pong_state_t pong;
        tetris_state_t tetris;
    } u;
} arcade_refresh_state_t;

/**********************
 *  STATIC PROTOTYPES
 **********************/
static lv_obj_t * arcade_create(lv_obj_t * parent, arcade_kind_t kind,
                                const char * title);
static void arcade_tick_cb(lv_timer_t * timer);
static void arcade_delete_cb(lv_event_t * e);
static void arcade_visibility_cb(lv_event_t * e);
static void arcade_board_draw_cb(lv_event_t * e);
static void arcade_status_update(arcade_game_t * game);
static void draw_cell(lv_layer_t * layer, const lv_area_t * board_area,
                      int32_t cols, int32_t rows, int32_t x, int32_t y,
                      lv_color_t color, lv_coord_t radius);

static void snake_reset(void * state);
static void snake_update(void * state);
static void snake_draw(const void * state, lv_layer_t * layer, const lv_area_t * area);
static void snake_status(const void * state, char * buf, size_t size);
static void snake_place_fruit(snake_state_t * s);

static void pong_reset(void * state);
static void pong_update(void * state);
static void pong_draw(const void * state, lv_layer_t * layer, const lv_area_t * area);
static void pong_status(const void * state, char * buf, size_t size);

static void tetris_reset(void * state);
static void tetris_update(void * state);
static void tetris_draw(const void * state, lv_layer_t * layer, const lv_area_t * area);
static void tetris_status(const void * state, char * buf, size_t size);
static bool tetris_piece_cell(int type, int rot, int r, int c);
static bool tetris_collides(const tetris_state_t * t,
                            int type, int rot, int px, int py);
static int tetris_field_height(const tetris_state_t * t);
static int tetris_count_holes(const tetris_state_t * t);
static int tetris_count_complete(const tetris_state_t * t);
static void tetris_ai_choose(tetris_state_t * t);
static void tetris_spawn(tetris_state_t * t);
static void tetris_clear_rows(tetris_state_t * t);

/**********************
 *  STATIC CONSTANTS
 **********************/
static const lv_color_t s_tetris_colors[7] = {
    LV_COLOR_MAKE(85, 255, 255),
    LV_COLOR_MAKE(255, 255, 85),
    LV_COLOR_MAKE(255, 85, 255),
    LV_COLOR_MAKE(85, 255, 85),
    LV_COLOR_MAKE(255, 85, 85),
    LV_COLOR_MAKE(255, 170, 0),
    LV_COLOR_MAKE(85, 85, 255),
};

static const uint16_t s_tetris_pieces[7][4] = {
    {0x0F00, 0x2222, 0x00F0, 0x4444},
    {0x6600, 0x6600, 0x6600, 0x6600},
    {0x0E40, 0x4C40, 0x4E00, 0x4640},
    {0x06C0, 0x8C40, 0x6C00, 0x4620},
    {0x0C60, 0x4C80, 0xC600, 0x2640},
    {0x0E80, 0xC440, 0x2E00, 0x44C0},
    {0x0E20, 0x44C0, 0x8E00, 0xC880},
};

static const arcade_ops_t s_ops[ARCADE_KIND_COUNT] = {
    [ARCADE_SNAKE]  = { snake_reset,  snake_update,  snake_draw,  snake_status,  120 },
    [ARCADE_PONG]   = { pong_reset,   pong_update,   pong_draw,   pong_status,    55 },
    [ARCADE_TETRIS] = { tetris_reset, tetris_update, tetris_draw, tetris_status,  55 },
};

#define ARCADE_REFRESH_STATE_MAGIC 0x41524344u

/* Buffer in lv_demo_cellphone.c is sized off the constant in the header.
 * If a state field grows past the budget, capture would silently return
 * false and theme-refresh would lose game progress; fail the build instead. */
_Static_assert(sizeof(arcade_refresh_state_t) <= CELLPHONE_GAME_REFRESH_STATE_SIZE,
               "CELLPHONE_GAME_REFRESH_STATE_SIZE too small for arcade_refresh_state_t");

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

lv_obj_t * cellphone_snake_create(lv_obj_t * parent)
{
    return arcade_create(parent, ARCADE_SNAKE, "Snake");
}

lv_obj_t * cellphone_pong_create(lv_obj_t * parent)
{
    return arcade_create(parent, ARCADE_PONG, "Pong");
}

lv_obj_t * cellphone_tetris_create(lv_obj_t * parent)
{
    return arcade_create(parent, ARCADE_TETRIS, "Tetris");
}

bool cellphone_game_refresh_state_capture(lv_obj_t * screen, void * buf, size_t size)
{
    arcade_game_t * game;
    arcade_refresh_state_t * state = buf;

    if(!screen || !buf || size < sizeof(*state)) return false;

    game = lv_obj_get_user_data(screen);
    if(!game) return false;

    state->magic = ARCADE_REFRESH_STATE_MAGIC;
    state->paused = game->timer ? lv_timer_get_paused(game->timer) : 1;
    state->kind = game->kind;
    lv_memcpy(&state->u, &game->u, sizeof(state->u));
    return true;
}

bool cellphone_game_refresh_state_restore(lv_obj_t * screen, const void * buf, size_t size)
{
    arcade_game_t * game;
    const arcade_refresh_state_t * state = buf;

    if(!screen || !buf || size < sizeof(*state)) return false;
    if(state->magic != ARCADE_REFRESH_STATE_MAGIC) return false;

    game = lv_obj_get_user_data(screen);
    if(!game || game->kind != state->kind) return false;

    lv_memcpy(&game->u, &state->u, sizeof(game->u));
    if(game->timer) {
        if(state->paused) lv_timer_pause(game->timer);
        else lv_timer_resume(game->timer);
    }
    arcade_status_update(game);
    if(game->board && lv_obj_is_valid(game->board)) lv_obj_invalidate(game->board);
    return true;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static lv_obj_t * arcade_create(lv_obj_t * parent, arcade_kind_t kind,
                                const char * title)
{
    arcade_game_t * game = lv_malloc_zeroed(sizeof(*game));
    if(!game) return parent;

    game->kind = kind;

    lv_obj_t * header = cellphone_section_header(parent, title,
                                                 LV_ALIGN_CENTER, "AI");
    lv_obj_set_pos(header, 0, 0);

    lv_obj_t * board = cellphone_obj_fill(parent, lv_color_hex(0x101614));
    game->board = board;
    lv_obj_set_size(board,
                    CELLPHONE_CONTENT_W - ARCADE_BOARD_MARGIN * 2,
                    CELLPHONE_CONTENT_H - CELLPHONE_SECTION_HDR_H - ARCADE_STATUS_H - 20);
    lv_obj_align(board, LV_ALIGN_TOP_MID, 0, CELLPHONE_SECTION_HDR_H + 10);
    lv_obj_set_style_radius(board, 14, 0);
    lv_obj_set_style_border_width(board, 1, 0);
    lv_obj_set_style_border_color(board, lv_color_hex(0x5c746b), 0);
    lv_obj_clear_flag(board, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(board, arcade_board_draw_cb, LV_EVENT_DRAW_MAIN, game);

    game->status = cellphone_label(parent, NULL, CELLPHONE_FONT_SM, CELLPHONE_COLOR_TEXT_SEC);
    lv_obj_align(game->status, LV_ALIGN_BOTTOM_MID, 0, -8);

    lv_obj_t * screen = lv_obj_get_parent(parent);
    lv_obj_set_user_data(screen, game);
    lv_obj_add_event_cb(screen, arcade_delete_cb, LV_EVENT_DELETE, game);
    lv_obj_add_event_cb(screen, arcade_visibility_cb,
                        (lv_event_code_t)cellphone_screen_event_shown(), game);
    lv_obj_add_event_cb(screen, arcade_visibility_cb,
                        (lv_event_code_t)cellphone_screen_event_hidden(), game);

    const arcade_ops_t * ops = &s_ops[kind];
    ops->reset(&game->u);
    game->timer = lv_timer_create(arcade_tick_cb, ops->period_ms, game);

    arcade_status_update(game);
    return parent;
}

static void arcade_tick_cb(lv_timer_t * timer)
{
    arcade_game_t * game = lv_timer_get_user_data(timer);
    if(!game) return;

    s_ops[game->kind].update(&game->u);
    arcade_status_update(game);
    if(game->board && lv_obj_is_valid(game->board)) lv_obj_invalidate(game->board);
}

static void arcade_delete_cb(lv_event_t * e)
{
    lv_obj_t * screen = lv_event_get_target_obj(e);
    arcade_game_t * game = lv_event_get_user_data(e);
    if(!game) return;
    if(screen) lv_obj_set_user_data(screen, NULL);
    if(game->timer) lv_timer_delete(game->timer);
    lv_free(game);
}

static void arcade_visibility_cb(lv_event_t * e)
{
    arcade_game_t * game = lv_event_get_user_data(e);
    if(!game || !game->timer) return;

    if(lv_event_get_code(e) == (lv_event_code_t)cellphone_screen_event_hidden()) {
        lv_timer_pause(game->timer);
    }
    else {
        lv_timer_resume(game->timer);
    }
}

static void arcade_board_draw_cb(lv_event_t * e)
{
    arcade_game_t * game = lv_event_get_user_data(e);
    lv_layer_t * layer = lv_event_get_layer(e);
    lv_obj_t * obj = lv_event_get_target(e);
    lv_area_t area;

    if(!game || !layer || !obj) return;
    lv_obj_get_coords(obj, &area);

    s_ops[game->kind].draw(&game->u, layer, &area);
}

static void arcade_status_update(arcade_game_t * game)
{
    char buf[64];
    s_ops[game->kind].status(&game->u, buf, sizeof(buf));
    lv_label_set_text(game->status, buf);
}

static void draw_cell(lv_layer_t * layer, const lv_area_t * board_area,
                      int32_t cols, int32_t rows, int32_t x, int32_t y,
                      lv_color_t color, lv_coord_t radius)
{
    if(x < 0 || y < 0 || x >= cols || y >= rows) return;

    int32_t board_w = lv_area_get_width(board_area) - 16;
    int32_t board_h = lv_area_get_height(board_area) - 16;
    int32_t cell_w = board_w / cols;
    int32_t cell_h = board_h / rows;
    lv_area_t cell;
    lv_draw_rect_dsc_t dsc;

    if(cell_w < 2 || cell_h < 2) return;

    int32_t grid_w = cell_w * cols;
    int32_t grid_h = cell_h * rows;
    int32_t grid_x = board_area->x1 + (lv_area_get_width(board_area) - grid_w) / 2;
    int32_t grid_y = board_area->y1 + (lv_area_get_height(board_area) - grid_h) / 2;

    cell.x1 = grid_x + x * cell_w + 1;
    cell.y1 = grid_y + y * cell_h + 1;
    cell.x2 = cell.x1 + cell_w - 3;
    cell.y2 = cell.y1 + cell_h - 3;

    lv_draw_rect_dsc_init(&dsc);
    dsc.bg_color = color;
    dsc.bg_opa = LV_OPA_COVER;
    dsc.radius = radius;
    lv_draw_rect(layer, &dsc, &cell);
}

static bool snake_safe(const snake_state_t * s, arcade_pos_t p)
{
    if(p.x < 0 || p.x >= SNAKE_COLS || p.y < 0 || p.y >= SNAKE_ROWS) return false;
    for(uint32_t i = 0; i < s->len; i++) {
        if(s->body[i].x == p.x && s->body[i].y == p.y) return false;
    }
    return true;
}

static void snake_place_fruit(snake_state_t * s)
{
    do {
        s->fruit.x = rand() % SNAKE_COLS;
        s->fruit.y = rand() % SNAKE_ROWS;
    } while(!snake_safe(s, s->fruit));
}

static void snake_reset(void * state)
{
    snake_state_t * s = state;
    s->len = 5;
    s->dir.x = 1;
    s->dir.y = 0;
    for(uint32_t i = 0; i < s->len; i++) {
        s->body[i].x = (int16_t)(SNAKE_COLS / 2 - (int32_t)i);
        s->body[i].y = SNAKE_ROWS / 2;
    }
    snake_place_fruit(s);
}

static void snake_update(void * state)
{
    snake_state_t * s = state;
    arcade_pos_t head = s->body[0];
    arcade_pos_t desired = s->dir;

    if(s->fruit.x != head.x) {
        desired.x = s->fruit.x > head.x ? 1 : -1;
        desired.y = 0;
    }
    else if(s->fruit.y != head.y) {
        desired.x = 0;
        desired.y = s->fruit.y > head.y ? 1 : -1;
    }

    arcade_pos_t next = { head.x + desired.x, head.y + desired.y };
    if(snake_safe(s, next)) {
        s->dir = desired;
    }
    else {
        static const arcade_pos_t dirs[] = { {1, 0}, {-1, 0}, {0, 1}, {0, -1} };
        for(uint32_t i = 0; i < 4; i++) {
            next.x = head.x + dirs[i].x;
            next.y = head.y + dirs[i].y;
            if(snake_safe(s, next)) {
                s->dir = dirs[i];
                break;
            }
        }
    }

    next.x = head.x + s->dir.x;
    next.y = head.y + s->dir.y;
    if(!snake_safe(s, next)) {
        s->resets++;
        snake_reset(s);
        return;
    }

    bool grow = next.x == s->fruit.x && next.y == s->fruit.y;
    if(grow && s->len < SNAKE_MAX_LEN) s->len++;

    for(int32_t i = (int32_t)s->len - 1; i > 0; i--) {
        s->body[i] = s->body[i - 1];
    }
    s->body[0] = next;

    if(grow) snake_place_fruit(s);
}

static void snake_draw(const void * state, lv_layer_t * layer,
                       const lv_area_t * area)
{
    const snake_state_t * s = state;
    draw_cell(layer, area, SNAKE_COLS, SNAKE_ROWS,
              s->fruit.x, s->fruit.y, lv_color_hex(0xe53935), 3);
    for(uint32_t i = 0; i < s->len; i++) {
        draw_cell(layer, area, SNAKE_COLS, SNAKE_ROWS,
                  s->body[i].x, s->body[i].y,
                  i == 0 ? lv_color_hex(0xc5e1a5) : lv_color_hex(0x66bb6a), 3);
    }
}

static void snake_status(const void * state, char * buf, size_t size)
{
    const snake_state_t * s = state;
    lv_snprintf(buf, size, "Auto-play  Length %u  Resets %u",
                (unsigned)s->len, (unsigned)s->resets);
}

static void pong_reset(void * state)
{
    pong_state_t * p = state;
    p->cols = 22;
    p->rows = 14;
    p->ball_x = p->cols / 2;
    p->ball_y = p->rows / 2;
    p->dir_x = 1;
    p->dir_y = 1;
    p->paddle_l = p->rows / 2;
    p->paddle_r = p->rows / 2;
}

static void pong_update(void * state)
{
    pong_state_t * p = state;
    p->ball_x += p->dir_x;
    p->ball_y += p->dir_y;

    if(p->ball_y < 0 || p->ball_y >= p->rows) {
        p->dir_y *= -1;
        p->ball_y += p->dir_y * 2;
    }

    if(p->ball_x == 1 &&
       p->ball_y >= p->paddle_l - 1 && p->ball_y <= p->paddle_l + 1) {
        p->dir_x = 1;
        p->rallies++;
    }
    if(p->ball_x == p->cols - 2 &&
       p->ball_y >= p->paddle_r - 1 && p->ball_y <= p->paddle_r + 1) {
        p->dir_x = -1;
        p->rallies++;
    }

    if(p->ball_x < 0 || p->ball_x >= p->cols) {
        pong_reset(p);
        return;
    }

    if(p->ball_y > p->paddle_l) p->paddle_l++;
    if(p->ball_y < p->paddle_l) p->paddle_l--;
    if(p->ball_y > p->paddle_r) p->paddle_r++;
    if(p->ball_y < p->paddle_r) p->paddle_r--;

    if(p->paddle_l < 1) p->paddle_l = 1;
    if(p->paddle_l > p->rows - 2) p->paddle_l = p->rows - 2;
    if(p->paddle_r < 1) p->paddle_r = 1;
    if(p->paddle_r > p->rows - 2) p->paddle_r = p->rows - 2;
}

static void pong_draw(const void * state, lv_layer_t * layer,
                      const lv_area_t * area)
{
    const pong_state_t * p = state;
    for(int32_t y = 0; y < p->rows; y += 2) {
        draw_cell(layer, area, p->cols, p->rows, p->cols / 2, y,
                  lv_color_hex(0x34524a), 2);
    }

    for(int32_t i = -1; i <= 1; i++) {
        draw_cell(layer, area, p->cols, p->rows, 0, p->paddle_l + i,
                  lv_color_white(), 2);
        draw_cell(layer, area, p->cols, p->rows, p->cols - 1, p->paddle_r + i,
                  lv_color_white(), 2);
    }

    draw_cell(layer, area, p->cols, p->rows, p->ball_x, p->ball_y,
              lv_color_hex(0x4dd0e1), 3);
}

static void pong_status(const void * state, char * buf, size_t size)
{
    const pong_state_t * p = state;
    lv_snprintf(buf, size, "Auto-play  Rallies %u", (unsigned)p->rallies);
}

static bool tetris_piece_cell(int type, int rot, int r, int c)
{
    uint16_t bits = s_tetris_pieces[type][rot];
    return (bits >> (15 - (r * 4 + c))) & 1;
}

static bool tetris_collides(const tetris_state_t * t,
                            int type, int rot, int px, int py)
{
    for(int r = 0; r < 4; r++) {
        for(int c = 0; c < 4; c++) {
            if(!tetris_piece_cell(type, rot, r, c)) continue;
            int fx = px + c;
            int fy = py + r;
            if(fx < 0 || fx >= TETRIS_W || fy >= TETRIS_H) return true;
            if(fy >= 0 && t->field[fy][fx]) return true;
        }
    }
    return false;
}

static int tetris_field_height(const tetris_state_t * t)
{
    int h = 0;
    for(int c = 0; c < TETRIS_W; c++) {
        for(int r = 0; r < TETRIS_H; r++) {
            if(t->field[r][c]) {
                int col_h = TETRIS_H - r;
                if(col_h > h) h = col_h;
                break;
            }
        }
    }
    return h;
}

static int tetris_count_holes(const tetris_state_t * t)
{
    int holes = 0;
    for(int c = 0; c < TETRIS_W; c++) {
        bool found = false;
        for(int r = 0; r < TETRIS_H; r++) {
            if(t->field[r][c]) found = true;
            else if(found) holes++;
        }
    }
    return holes;
}

static int tetris_count_complete(const tetris_state_t * t)
{
    int lines = 0;
    for(int r = 0; r < TETRIS_H; r++) {
        bool full = true;
        for(int c = 0; c < TETRIS_W; c++) {
            if(!t->field[r][c]) {
                full = false;
                break;
            }
        }
        if(full) lines++;
    }
    return lines;
}

static void tetris_ai_choose(tetris_state_t * t)
{
    int best_score = -100000;
    int best_x = t->piece_x;
    int best_rot = t->rotation;

    for(int rot = 0; rot < 4; rot++) {
        for(int x = -2; x < TETRIS_W; x++) {
            if(tetris_collides(t, t->piece, rot, x, t->piece_y)) continue;

            int y = t->piece_y;
            while(!tetris_collides(t, t->piece, rot, x, y + 1)) y++;

            for(int r = 0; r < 4; r++) {
                for(int c = 0; c < 4; c++) {
                    if(tetris_piece_cell(t->piece, rot, r, c) &&
                       y + r >= 0 && y + r < TETRIS_H &&
                       x + c >= 0 && x + c < TETRIS_W) {
                        t->field[y + r][x + c] = 1;
                    }
                }
            }

            int score = tetris_count_complete(t) * 100
                        - tetris_field_height(t) * 10
                        - tetris_count_holes(t) * 30;

            for(int r = 0; r < 4; r++) {
                for(int c = 0; c < 4; c++) {
                    if(tetris_piece_cell(t->piece, rot, r, c) &&
                       y + r >= 0 && y + r < TETRIS_H &&
                       x + c >= 0 && x + c < TETRIS_W) {
                        t->field[y + r][x + c] = 0;
                    }
                }
            }

            if(score > best_score) {
                best_score = score;
                best_x = x;
                best_rot = rot;
            }
        }
    }

    t->target_x = (int8_t)best_x;
    t->target_rot = (int8_t)best_rot;
}

static void tetris_spawn(tetris_state_t * t)
{
    t->piece = rand() % 7;
    t->rotation = 0;
    t->piece_x = TETRIS_W / 2 - 2;
    t->piece_y = -1;
    tetris_ai_choose(t);
}

static void tetris_clear_rows(tetris_state_t * t)
{
    for(int r = TETRIS_H - 1; r >= 0; r--) {
        bool full = true;
        for(int c = 0; c < TETRIS_W; c++) {
            if(!t->field[r][c]) {
                full = false;
                break;
            }
        }

        if(full) {
            t->lines++;
            for(int rr = r; rr > 0; rr--) {
                for(int c = 0; c < TETRIS_W; c++) {
                    t->field[rr][c] = t->field[rr - 1][c];
                    t->color[rr][c] = t->color[rr - 1][c];
                }
            }
            lv_memzero(t->field[0], sizeof(t->field[0]));
            lv_memzero(t->color[0], sizeof(t->color[0]));
            r++;
        }
    }
}

static void tetris_reset(void * state)
{
    tetris_state_t * t = state;
    lv_memzero(t->field, sizeof(t->field));
    lv_memzero(t->color, sizeof(t->color));
    t->tick = 0;
    tetris_spawn(t);
}

static void tetris_update(void * state)
{
    tetris_state_t * t = state;
    t->tick++;
    if(t->tick < TETRIS_DROP_TICKS) return;
    t->tick = 0;

    if(t->rotation != t->target_rot) {
        int nr = (t->rotation + 1) % 4;
        if(!tetris_collides(t, t->piece, nr, t->piece_x, t->piece_y)) {
            t->rotation = (int8_t)nr;
        }
    }

    if(t->piece_x < t->target_x &&
       !tetris_collides(t, t->piece, t->rotation, t->piece_x + 1, t->piece_y)) {
        t->piece_x++;
    }
    else if(t->piece_x > t->target_x &&
            !tetris_collides(t, t->piece, t->rotation, t->piece_x - 1, t->piece_y)) {
        t->piece_x--;
    }

    if(!tetris_collides(t, t->piece, t->rotation, t->piece_x, t->piece_y + 1)) {
        t->piece_y++;
        return;
    }

    for(int r = 0; r < 4; r++) {
        for(int c = 0; c < 4; c++) {
            if(!tetris_piece_cell(t->piece, t->rotation, r, c)) continue;
            int fy = t->piece_y + r;
            int fx = t->piece_x + c;
            if(fy >= 0 && fy < TETRIS_H && fx >= 0 && fx < TETRIS_W) {
                t->field[fy][fx] = 1;
                t->color[fy][fx] = (uint8_t)t->piece;
            }
        }
    }

    tetris_clear_rows(t);
    tetris_spawn(t);
    if(tetris_collides(t, t->piece, t->rotation, t->piece_x, t->piece_y)) {
        t->resets++;
        tetris_reset(t);
    }
}

static void tetris_draw(const void * state, lv_layer_t * layer,
                        const lv_area_t * area)
{
    const tetris_state_t * t = state;
    for(int y = 0; y < TETRIS_H; y++) {
        for(int x = 0; x < TETRIS_W; x++) {
            if(!t->field[y][x]) continue;
            draw_cell(layer, area, TETRIS_W + 2, TETRIS_H, x + 1, y,
                      s_tetris_colors[t->color[y][x]], 2);
        }
    }

    for(int r = 0; r < 4; r++) {
        for(int c = 0; c < 4; c++) {
            if(!tetris_piece_cell(t->piece, t->rotation, r, c)) continue;
            int gx = t->piece_x + c;
            int gy = t->piece_y + r;
            if(gx >= 0 && gx < TETRIS_W && gy >= 0 && gy < TETRIS_H) {
                draw_cell(layer, area, TETRIS_W + 2, TETRIS_H, gx + 1, gy,
                          s_tetris_colors[t->piece], 2);
            }
        }
    }

    for(int y = 0; y < TETRIS_H; y++) {
        draw_cell(layer, area, TETRIS_W + 2, TETRIS_H, 0, y,
                  lv_color_hex(0x607d74), 1);
        draw_cell(layer, area, TETRIS_W + 2, TETRIS_H, TETRIS_W + 1, y,
                  lv_color_hex(0x607d74), 1);
    }
}

static void tetris_status(const void * state, char * buf, size_t size)
{
    const tetris_state_t * t = state;
    lv_snprintf(buf, size, "Auto-play  Lines %u  Resets %u",
                (unsigned)t->lines, (unsigned)t->resets);
}

#endif
