#include QMK_KEYBOARD_H
#include "tap_dance_override.h"

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
