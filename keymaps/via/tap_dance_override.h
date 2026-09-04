#pragma once

#include "process_tap_dance.h"

void circuitcurios_td_layer_move_on_each_tap(tap_dance_state_t *state, void *user_data);
void circuitcurios_td_layer_move_on_each_release(tap_dance_state_t *state, void *user_data);
void circuitcurios_td_layer_move_finished(tap_dance_state_t *state, void *user_data);
void circuitcurios_td_layer_move_reset(tap_dance_state_t *state, void *user_data);

// QMK's stock ACTION_TAP_DANCE_LAYER_MOVE delays the single-tap keycode until
// the dance has resolved. That is fine for ordinary keys, but it makes Ctrl,
// Shift and Alt awkward as chord modifiers. Keep stock behavior for ordinary
// keys, while modifier variants become active immediately on the first press.
#undef ACTION_TAP_DANCE_LAYER_MOVE
#define ACTION_TAP_DANCE_LAYER_MOVE(kc, layer)                                                                                \
    {                                                                                                                         \
        .fn = {                                                                                                               \
            circuitcurios_td_layer_move_on_each_tap,                                                                          \
            circuitcurios_td_layer_move_finished,                                                                             \
            circuitcurios_td_layer_move_reset,                                                                                \
            circuitcurios_td_layer_move_on_each_release                                                                       \
        },                                                                                                                    \
        .user_data = (void *)&((tap_dance_dual_role_t){kc, layer, layer_move}),                                               \
    }
