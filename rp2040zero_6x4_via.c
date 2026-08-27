#include QMK_KEYBOARD_H

// This macropad uses layer 0 as its one and only base layer.
// QMK's TO(layer) intentionally leaves the default layer active, so a stale
// DF()/PDF()/EEPROM default on layer 1+ can sit above TO(0) and make it look
// as if TO(0) does not work. Clamp the default-layer stack to layer 0 for all
// keymaps while leaving the normal overlay layer_state free for TO/MO/etc.
layer_state_t default_layer_state_set_kb(layer_state_t state) {
    (void)state;
    return default_layer_state_set_user((layer_state_t)1 << 0);
}
