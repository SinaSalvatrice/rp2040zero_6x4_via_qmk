#include QMK_KEYBOARD_H

#ifdef VIA_ENABLE
#    include "via.h"
#    include "dynamic_keymap.h"
#endif

#ifdef VIA_ENABLE
static bool via_key_is_locked(uint8_t layer, uint8_t row, uint8_t column) {
    return layer <= 2 && row == (MATRIX_ROWS - 1) && column < MATRIX_COLS;
}

static uint16_t via_key_byte_offset(uint8_t layer, uint8_t row, uint8_t column) {
    return (uint16_t)((((uint16_t)layer * MATRIX_ROWS + row) * MATRIX_COLS + column) * 2U);
}

// VIA has no native per-position read-only flag. Instead, sanitize incoming
// dynamic-keymap writes before QMK's normal VIA handler stores them. The last
// matrix row (R5) on layers 0, 1, and 2 therefore remains visible in VIA but
// cannot be changed there. Both single-key writes and bulk keymap writes are
// protected.
bool via_command_kb(uint8_t *data, uint8_t length) {
    if (data == NULL || length == 0) {
        return false;
    }

    if (data[0] == id_dynamic_keymap_set_keycode && length >= 6) {
        uint8_t layer  = data[1];
        uint8_t row    = data[2];
        uint8_t column = data[3];

        if (via_key_is_locked(layer, row, column)) {
            uint16_t current = dynamic_keymap_get_keycode(layer, row, column);
            data[4] = (uint8_t)(current >> 8);
            data[5] = (uint8_t)(current & 0xFF);
        }
        return false;
    }

    if (data[0] == id_dynamic_keymap_set_buffer && length >= 4) {
        uint16_t offset = ((uint16_t)data[1] << 8) | data[2];
        uint8_t size = data[3];
        uint8_t max_payload = (uint8_t)(length - 4);
        if (size > max_payload) {
            size = max_payload;
        }

        uint16_t end = offset + size;

        for (uint8_t layer = 0; layer <= 2; layer++) {
            uint8_t row = MATRIX_ROWS - 1;
            for (uint8_t column = 0; column < MATRIX_COLS; column++) {
                uint16_t key_offset = via_key_byte_offset(layer, row, column);
                uint16_t current = dynamic_keymap_get_keycode(layer, row, column);

                if (key_offset >= offset && key_offset < end) {
                    data[4 + (key_offset - offset)] = (uint8_t)(current >> 8);
                }
                if ((key_offset + 1U) >= offset && (key_offset + 1U) < end) {
                    data[4 + (key_offset + 1U - offset)] = (uint8_t)(current & 0xFF);
                }
            }
        }
    }

    // Return false so QMK's standard VIA handler processes and replies to the
    // (possibly sanitized) command normally.
    return false;
}
#endif

bool process_record_kb(uint16_t keycode, keyrecord_t *record) {
    // Keep the keymap-level processing, including positional RGB and keymap
    // actions, in the normal QMK chain.
    if (!process_record_user(keycode, record)) {
        return false;
    }

    if (!record->event.pressed) {
        return true;
    }

    // QK_KB_0.. map 1:1 to VIA customKeycodes[0..]. This gives the shortcuts
    // friendly names in VIA while keeping their actual Windows chords here.
    switch (keycode) {
        case QK_KB_0: // Screenshot / Snipping Tool
            tap_code16(LGUI(LSFT(KC_S)));
            return false;
        case QK_KB_1: // Clipboard history
            tap_code16(LGUI(KC_V));
            return false;
        case QK_KB_2: // Task Manager
            tap_code16(LCTL(LSFT(KC_ESC)));
            return false;
        case QK_KB_3: // File Explorer
            tap_code16(LGUI(KC_E));
            return false;
        case QK_KB_4: // Run dialog
            tap_code16(LGUI(KC_R));
            return false;
        case QK_KB_5: // Close active window
            tap_code16(LALT(KC_F4));
            return false;
        case QK_KB_6: // Show desktop
            tap_code16(LGUI(KC_D));
            return false;
        case QK_KB_7: // Screen recording toggle
            tap_code16(LGUI(LALT(KC_R)));
            return false;
        default:
            return true;
    }
}
