#pragma once

#define ENCODER_BTN_PIN GP10

// Explicit WS2812 / RGBLight configuration.
// Keep this in config.h so the LED hardware does not depend only on generated info.json defines.
#define WS2812_DI_PIN GP29
#define RGBLIGHT_LED_COUNT 21
#define RGBLIGHT_LIMIT_VAL 120
#define RGBLIGHT_DEFAULT_HUE 149
#define RGBLIGHT_DEFAULT_SAT 255
#define RGBLIGHT_DEFAULT_VAL 40

#define USB_SUSPEND_WAKEUP_DELAY 0
#define DEBOUNCE 5
