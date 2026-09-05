#pragma once

// RGB Matrix defaults. WS2812 pin and LED layout live in keyboard.json.
#define RGB_MATRIX_MAXIMUM_BRIGHTNESS 120
#define RGB_MATRIX_DEFAULT_HUE 149
#define RGB_MATRIX_DEFAULT_SAT 255
#define RGB_MATRIX_DEFAULT_VAL 40
#define RGB_MATRIX_DEFAULT_SPD 32

// Generic positional RGB mapping.
// Key and LED coordinates are defined independently in positional_rgb.h.
#define POSITIONAL_RGB_LED_COUNT 30
#define POSITIONAL_RGB_REACTIVE_RADIUS 110
#define POSITIONAL_RGB_REACTIVE_FALLOFF 2
#define POSITIONAL_RGB_REACTIVE_BRIGHTNESS 255
#define POSITIONAL_RGB_DECAY_MIN_MS 180
#define POSITIONAL_RGB_DECAY_MAX_MS 900
#define POSITIONAL_RGB_EVENT_HISTORY 16

// VIA custom layer-lighting UI + persistent settings.
// Each of the five layers stores two independently selectable colors,
// an effect and its speed.
#define VIA_FIRMWARE_VERSION 9
#define EECONFIG_USER_DATA_SIZE 32
#define EECONFIG_USER_DATA_VERSION 5

#define USB_SUSPEND_WAKEUP_DELAY 0
#define DEBOUNCE 5
