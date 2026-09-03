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
