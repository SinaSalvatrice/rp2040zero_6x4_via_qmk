#include QMK_KEYBOARD_H

// This macropad uses layer 0 as its one and only base layer.
// Keep the default-layer stack pinned to layer 0.
layer_state_t default_layer_state_set_kb(layer_state_t state) {
    (void)state;
    return default_layer_state_set_user((layer_state_t)1 << 0);
}

// Make TO(0) a hard return-to-base command.
// This runs before VIA and QMK's normal TO() processing, so it also works
// with dynamically stored VIA keymaps. R0/C1 is additionally treated as the
// dedicated return key on every non-base layer, even if stale VIA EEPROM data
// has a different keycode stored there.
bool process_record_kb(uint16_t keycode, keyrecord_t *record) {
    if (record->event.pressed) {
        bool is_to_base = keycode >= QK_TO && keycode <= QK_TO_MAX && QK_TO_GET_LAYER(keycode) == 0;
        bool is_physical_return_key = record->event.key.row == 0 && record->event.key.col == 1 && get_highest_layer(layer_state | default_layer_state) != 0;

        if (is_to_base || is_physical_return_key) {
            set_single_default_layer(0);
            layer_move(0);
            return false;
        }
    }

    return process_record_user(keycode, record);
}
