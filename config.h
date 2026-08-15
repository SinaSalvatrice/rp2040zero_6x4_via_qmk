#pragma once

#define ENCODER_BTN_PIN GP10

// RGB Matrix defaults. WS2812 pin and LED layout live in keyboard.json.
#define RGB_MATRIX_MAXIMUM_BRIGHTNESS 120
#define RGB_MATRIX_STARTUP_HUE 149
#define RGB_MATRIX_STARTUP_SAT 255
#define RGB_MATRIX_STARTUP_VAL 40
#define RGB_MATRIX_STARTUP_SPD 128

// VIA custom layer-color UI + persistent settings.
#define VIA_FIRMWARE_VERSION 1
#define EECONFIG_USER_DATA_SIZE 16
#define EECONFIG_USER_DATA_VERSION 1

#define USB_SUSPEND_WAKEUP_DELAY 0
#define DEBOUNCE 5
