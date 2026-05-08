/**
 * @file lv_demo_cellphone_softkbd.h
 *
 * Soft keyboard for the Cell Phone Demo, modeled after the show/hide
 * lifecycle of mg-demos/softkbd but built on lv_keyboard.  The keyboard
 * is anchored to the bottom of a host content area and is shown only
 * while a textarea is being edited.
 */

#ifndef LV_DEMO_CELLPHONE_SOFTKBD_H
#define LV_DEMO_CELLPHONE_SOFTKBD_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lv_demo_cellphone_common.h"

#if LV_USE_DEMO_CELLPHONE

/** Height of the soft keyboard in pixels.  The compact layout keeps four
 *  full rows but trims the chrome around them so chat history keeps more
 *  vertical room.  QVGA still lands around a ~35 px touch band per row,
 *  which is the smallest size that remained reliable in-thumb. */
#if LV_DEMO_CELLPHONE_LARGE
#define CELLPHONE_SOFTKBD_H 132
#else
#define CELLPHONE_SOFTKBD_H 144
#endif

/**
 * Show the soft keyboard at the bottom of @p host_parent and attach it
 * to @p ta.  The keyboard is lazy-created on first call, reused on
 * subsequent calls, and reparented if @p host_parent changes.  The
 * textarea receives LV_EVENT_READY on OK and LV_EVENT_CANCEL on the
 * hide-keyboard key.
 */
void cellphone_softkbd_show(lv_obj_t * host_parent, lv_obj_t * ta);

/**
 * Hide the soft keyboard with a slide-out animation.  Idempotent.
 */
void cellphone_softkbd_hide(void);

/**
 * @return true while the keyboard is mounted and not animating out.
 */
bool cellphone_softkbd_is_visible(void);

/**
 * @return the keyboard's nominal height (CELLPHONE_SOFTKBD_H).
 */
int32_t cellphone_softkbd_height(void);

/**
 * Callback fired after the keyboard finishes its slide-out animation
 * (so the host can reflow its layout without jumping under a still-
 * visible keyboard).  Pass NULL to clear.
 */
typedef void (*cellphone_softkbd_hidden_cb_t)(void);
void cellphone_softkbd_set_hidden_cb(cellphone_softkbd_hidden_cb_t cb);

#endif /* LV_USE_DEMO_CELLPHONE */

#ifdef __cplusplus
}
#endif

#endif /* LV_DEMO_CELLPHONE_SOFTKBD_H */
