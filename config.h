#pragma once

#define ENCODER_BTN_PIN GP10

// RGB Matrix defaults. WS2812 pin and LED layout live in keyboard.json.
#define RGB_MATRIX_MAXIMUM_BRIGHTNESS 120
#define RGB_MATRIX_DEFAULT_HUE 149
#define RGB_MATRIX_DEFAULT_SAT 255
#define RGB_MATRIX_DEFAULT_VAL 40
#define RGB_MATRIX_DEFAULT_SPD 32

// VIA custom layer-lighting UI + persistent settings.
// Each of the five layers stores two independently selectable colors,
// an effect and its speed.
#define VIA_FIRMWARE_VERSION 7
#define EECONFIG_USER_DATA_SIZE 32
#define EECONFIG_USER_DATA_VERSION 4

#define USB_SUSPEND_WAKEUP_DELAY 0
#define DEBOUNCE 5
