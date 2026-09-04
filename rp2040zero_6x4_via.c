#include QMK_KEYBOARD_H
#include "bootloader.h"
#include "wait.h"

void keyboard_pre_init_kb(void) {
#ifdef ENCODER_BTN_PIN
    // Keymap-independent recovery path: hold the encoder button while the
    // keyboard powers up to enter the RP2040 bootloader. This runs before VIA,
    // the dynamic keymap, layers, or RGB profiles are initialized.
    gpio_set_pin_input_high(ENCODER_BTN_PIN);
    wait_ms(20);

    if (!gpio_read_pin(ENCODER_BTN_PIN)) {
        bootloader_jump();
    }
#endif

    keyboard_pre_init_user();
}

bool process_record_kb(uint16_t keycode, keyrecord_t *record) {
    // Keep the keymap-level processing (including positional RGB and the
    // protected recovery keycodes) in the normal QMK chain.
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
