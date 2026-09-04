#include QMK_KEYBOARD_H
#include "dynamic_keymap.h"
#include "eeconfig.h"
#include "tap_dance_override.h"

#define R3C1_ENTER_MARKER 0xE1
#define R3C1_ENTER_MARKER_OFFSET (EECONFIG_USER_DATA_SIZE - 1)

static bool circuitcurios_td_is_modifier(tap_dance_dual_role_t *action) {
    return IS_MODIFIER_KEYCODE(action->kc);
}

void circuitcurios_td_layer_move_on_each_tap(tap_dance_state_t *state, void *user_data) {
    tap_dance_dual_role_t *action = (tap_dance_dual_role_t *)user_data;

    if (!circuitcurios_td_is_modifier(action)) {
        tap_dance_dual_role_on_each_tap(state, user_data);
        return;
    }

    if (state->count == 1) {
        // Modifier must be active immediately so chords such as Ctrl+Z work
        // while this same key can still be double-tapped for layer switching.
        register_code16(action->kc);
        return;
    }

    if (state->count == 2) {
        unregister_code16(action->kc);
        action->layer_function(action->layer);
        state->finished = true;
    }
}

void circuitcurios_td_layer_move_on_each_release(tap_dance_state_t *state, void *user_data) {
    (void)state;
    tap_dance_dual_role_t *action = (tap_dance_dual_role_t *)user_data;

    if (circuitcurios_td_is_modifier(action)) {
        unregister_code16(action->kc);
    }
}

void circuitcurios_td_layer_move_finished(tap_dance_state_t *state, void *user_data) {
    tap_dance_dual_role_t *action = (tap_dance_dual_role_t *)user_data;

    if (circuitcurios_td_is_modifier(action)) {
        // The modifier was already registered on the first physical press.
        return;
    }

    tap_dance_dual_role_finished(state, user_data);
}

void circuitcurios_td_layer_move_reset(tap_dance_state_t *state, void *user_data) {
    tap_dance_dual_role_t *action = (tap_dance_dual_role_t *)user_data;

    if (circuitcurios_td_is_modifier(action)) {
        unregister_code16(action->kc);
        return;
    }

    tap_dance_dual_role_reset(state, user_data);
}

void housekeeping_task_user(void) {
    static bool checked = false;
    if (checked || !eeconfig_is_user_datablock_valid()) {
        return;
    }

    uint8_t marker = 0;
    eeconfig_read_user_datablock(&marker, R3C1_ENTER_MARKER_OFFSET, sizeof(marker));

    if (marker != R3C1_ENTER_MARKER) {
        // R3/C1 on Creative used to be keypad Enter. Only migrate that exact
        // legacy value; a VIA assignment the user has already changed is kept.
        if (dynamic_keymap_get_keycode(0, 3, 1) == KC_PENT) {
            dynamic_keymap_set_keycode(0, 3, 1, KC_ENT);
        }

        marker = R3C1_ENTER_MARKER;
        eeconfig_update_user_datablock(&marker, R3C1_ENTER_MARKER_OFFSET, sizeof(marker));
    }

    checked = true;
}
