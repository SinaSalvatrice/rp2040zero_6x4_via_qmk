#pragma once

#define ENCODER_BTN_PIN GP12

#define WS2812_DI_PIN GP13
#define RGBLIGHT_LED_COUNT 10
#define RGBLIGHT_LIMIT_VAL 120
#define RGBLIGHT_DEFAULT_HUE 128
#define RGBLIGHT_DEFAULT_SAT 255
#define RGBLIGHT_DEFAULT_VAL 40
#define RGBLIGHT_SLEEP

#define USB_SUSPEND_WAKEUP_DELAY 0
#define DEBOUNCE 5

// Vial requires a unique keyboard UID and an unlock chord.
#define VIAL_KEYBOARD_UID {0x59, 0xF2, 0x2D, 0x4B, 0x71, 0xA6, 0xC3, 0x9E}
#define VIAL_UNLOCK_COMBO_ROWS {0, 0}
#define VIAL_UNLOCK_COMBO_COLS {0, 1}
