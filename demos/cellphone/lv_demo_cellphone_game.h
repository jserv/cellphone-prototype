/**
 * @file lv_demo_cellphone_game.h
 */

#ifndef LV_DEMO_CELLPHONE_GAME_H
#define LV_DEMO_CELLPHONE_GAME_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lv_demo_cellphone_common.h"

#if LV_USE_DEMO_CELLPHONE

#define CELLPHONE_GAME_REFRESH_STATE_SIZE 416

lv_obj_t * cellphone_snake_create(lv_obj_t * parent);
lv_obj_t * cellphone_pong_create(lv_obj_t * parent);
lv_obj_t * cellphone_tetris_create(lv_obj_t * parent);
bool cellphone_game_refresh_state_capture(lv_obj_t * screen, void * buf, size_t size);
bool cellphone_game_refresh_state_restore(lv_obj_t * screen, const void * buf, size_t size);

#endif

#ifdef __cplusplus
}
#endif

#endif
