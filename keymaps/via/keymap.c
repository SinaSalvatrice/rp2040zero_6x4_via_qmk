#include QMK_KEYBOARD_H
#include "eeconfig.h"
#include "timer.h"
#include "via.h"
#include "positional_rgb.h"

#define LAYER_COUNT 5
#define LAYER_EFFECT_COUNT 23

enum layer_names {
    _CREATIVE,
    _NUMPAD,
    _NAV,
    _GRAPHICS,
    _SETTINGS
};

enum custom_keycodes {
    SAFE_BOOT = SAFE_RANGE,
    SAFE_EEPROM_RESET
};

enum via_rgb_ui_value {
    id_layer_color_a = 1,
    id_layer_color_b,
    id_layer_effect,
    id_layer_speed
};

enum layer_effects {
    LFX_SOLID = 0,
    LFX_BREATHING,
    LFX_GRADIENT_LEFT_RIGHT,
    LFX_CYCLE_ALL,
    LFX_RAINBOW_LEFT_RIGHT,
    LFX_RAINBOW_UP_DOWN,
    LFX_RAINBOW_CHEVRON,
    LFX_RAINBOW_OUT_IN,
    LFX_RAINBOW_OUT_IN_DUAL,
    LFX_RAINBOW_PINWHEEL,
    LFX_RAINBOW_SPIRAL,
    LFX_DUAL_BEACON,
    LFX_RAINBOW_BEACON,
    LFX_JELLYBEAN_RAINDROPS,
    LFX_HUE_PENDULUM,
    LFX_PIXEL_RAIN,
    LFX_REACTIVE_SIMPLE,
    LFX_REACTIVE_WIDE,
    LFX_SPLASH,
    LFX_MULTISPLASH,
    LFX_DUAL_GRADIENT,
    LFX_DUAL_BREATH,
    LFX_DUAL_WAVE
};

typedef struct {
    uint8_t layer_hue_a[LAYER_COUNT];
    uint8_t layer_sat_a[LAYER_COUNT];
    uint8_t layer_hue_b[LAYER_COUNT];
    uint8_t layer_sat_b[LAYER_COUNT];
    uint8_t layer_effect[LAYER_COUNT];
    uint8_t layer_speed[LAYER_COUNT];
} rgb_ui_config_t;

static rgb_ui_config_t rgb_cfg = {
    .layer_hue_a  = {149, 64, 170, 155, 0},
    .layer_sat_a  = {255, 255, 255, 255, 255},
    .layer_hue_b  = {190, 170, 210, 96, 160},
    .layer_sat_b  = {255, 255, 255, 255, 255},
    .layer_effect = {LFX_DUAL_GRADIENT, LFX_BREATHING, LFX_REACTIVE_SIMPLE, LFX_DUAL_WAVE, LFX_SOLID},
    .layer_speed  = {32, 24, 64, 40, 32}
};

static uint8_t last_layer = _CREATIVE;
static bool encoder_btn_pressed = false;
static bool encoder_btn_consumed = false;
static uint16_t encoder_btn_tmr = 0;

static uint8_t clamp_layer(uint8_t layer) {
    return layer < LAYER_COUNT ? layer : _CREATIVE;
}

static uint8_t clamp_effect(uint8_t effect) {
    return effect < LAYER_EFFECT_COUNT ? effect : LFX_SOLID;
}

static bool is_dual_effect(uint8_t effect) {
    return effect >= LFX_DUAL_GRADIENT && effect <= LFX_DUAL_WAVE;
}

static bool encoder_button_is_pressed(void) {
#ifdef ENCODER_BTN_PIN
    return !gpio_read_pin(ENCODER_BTN_PIN);
#else
    return false;
#endif
}

static uint8_t triangle8(uint8_t value) {
    if (value < 128) {
        return value * 2;
    }
    return (255 - value) * 2;
}

static uint8_t blend8(uint8_t a, uint8_t b, uint8_t amount) {
    int16_t delta = (int16_t)b - (int16_t)a;
    return (uint8_t)((int16_t)a + (delta * amount) / 255);
}

static rgb_t blend_rgb(rgb_t a, rgb_t b, uint8_t amount) {
    rgb_t out = {
        .r = blend8(a.r, b.r, amount),
        .g = blend8(a.g, b.g, amount),
        .b = blend8(a.b, b.b, amount)
    };
    return out;
}

static rgb_t layer_color(uint8_t hue, uint8_t sat, uint8_t val) {
    hsv_t hsv = {.h = hue, .s = sat, .v = val};
    return hsv_to_rgb(hsv);
}

static uint32_t animation_cycle_ms(uint8_t speed) {
    return 14000U - ((uint32_t)speed * 44U);
}

static void rgb_ui_save(void) {
    eeconfig_update_user_datablock(&rgb_cfg, 0, sizeof(rgb_cfg));
}

static void rgb_ui_load(void) {
    if (!eeconfig_is_user_datablock_valid()) {
        eeconfig_init_user_datablock();
        rgb_ui_save();
        return;
    }

    eeconfig_read_user_datablock(&rgb_cfg, 0, sizeof(rgb_cfg));
    for (uint8_t i = 0; i < LAYER_COUNT; i++) {
        rgb_cfg.layer_effect[i] = clamp_effect(rgb_cfg.layer_effect[i]);
    }
}

static uint8_t qmk_mode_for_effect(uint8_t effect) {
    switch (effect) {
        case LFX_BREATHING:
            return RGB_MATRIX_BREATHING;
        case LFX_GRADIENT_LEFT_RIGHT:
            return RGB_MATRIX_GRADIENT_LEFT_RIGHT;
        case LFX_CYCLE_ALL:
            return RGB_MATRIX_CYCLE_ALL;
        case LFX_RAINBOW_LEFT_RIGHT:
            return RGB_MATRIX_CYCLE_LEFT_RIGHT;
        case LFX_RAINBOW_UP_DOWN:
            return RGB_MATRIX_CYCLE_UP_DOWN;
        case LFX_RAINBOW_CHEVRON:
            return RGB_MATRIX_RAINBOW_MOVING_CHEVRON;
        case LFX_RAINBOW_OUT_IN:
            return RGB_MATRIX_CYCLE_OUT_IN;
        case LFX_RAINBOW_OUT_IN_DUAL:
            return RGB_MATRIX_CYCLE_OUT_IN_DUAL;
        case LFX_RAINBOW_PINWHEEL:
            return RGB_MATRIX_CYCLE_PINWHEEL;
        case LFX_RAINBOW_SPIRAL:
            return RGB_MATRIX_CYCLE_SPIRAL;
        case LFX_DUAL_BEACON:
            return RGB_MATRIX_DUAL_BEACON;
        case LFX_RAINBOW_BEACON:
            return RGB_MATRIX_RAINBOW_BEACON;
        case LFX_JELLYBEAN_RAINDROPS:
            return RGB_MATRIX_JELLYBEAN_RAINDROPS;
        case LFX_HUE_PENDULUM:
            return RGB_MATRIX_HUE_PENDULUM;
        case LFX_PIXEL_RAIN:
            return RGB_MATRIX_PIXEL_RAIN;
        case LFX_REACTIVE_SIMPLE:
            return RGB_MATRIX_SOLID_REACTIVE_SIMPLE;
        case LFX_REACTIVE_WIDE:
            return RGB_MATRIX_SOLID_REACTIVE_WIDE;
        case LFX_SPLASH:
            return RGB_MATRIX_SPLASH;
        case LFX_MULTISPLASH:
            return RGB_MATRIX_MULTISPLASH;
        case LFX_SOLID:
        default:
            return RGB_MATRIX_SOLID_COLOR;
    }
}

static void apply_layer_profile(uint8_t layer) {
    uint8_t safe_layer = clamp_layer(layer);
    uint8_t effect = clamp_effect(rgb_cfg.layer_effect[safe_layer]);
    hsv_t hsv = rgb_matrix_get_hsv();

    rgb_matrix_sethsv_noeeprom(rgb_cfg.layer_hue_a[safe_layer], rgb_cfg.layer_sat_a[safe_layer], hsv.v);
    rgb_matrix_set_speed_noeeprom(rgb_cfg.layer_speed[safe_layer]);

    if (is_dual_effect(effect) || effect == LFX_REACTIVE_SIMPLE) {
        // Custom renderers own the LED output in the indicators callback.
        rgb_matrix_mode_noeeprom(RGB_MATRIX_SOLID_COLOR);
    } else {
        rgb_matrix_mode_noeeprom(qmk_mode_for_effect(effect));
    }
}

static void handle_encoder_button_tap(void) {
    if (last_layer == _GRAPHICS) {
        tap_code(KC_Q);
    }
}

void keyboard_post_init_user(void) {
#ifdef ENCODER_BTN_PIN
    gpio_set_pin_input_high(ENCODER_BTN_PIN);
#endif

    positional_rgb_reset_state();
    rgb_ui_load();
    last_layer = clamp_layer(get_highest_layer(layer_state | default_layer_state));
    apply_layer_profile(last_layer);
}

void matrix_scan_user(void) {
    if (!rgb_matrix_is_enabled()) {
        rgb_matrix_enable_noeeprom();
    }

#ifdef ENCODER_BTN_PIN
    if (timer_elapsed(encoder_btn_tmr) < 10) {
        return;
    }

    bool pressed = encoder_button_is_pressed();
    if (pressed == encoder_btn_pressed) {
        return;
    }

    encoder_btn_tmr = timer_read();
    encoder_btn_pressed = pressed;

    if (pressed) {
        encoder_btn_consumed = false;
    } else if (!encoder_btn_consumed) {
        handle_encoder_button_tap();
    }
#endif
}

layer_state_t layer_state_set_user(layer_state_t state) {
    uint8_t layer = clamp_layer(get_highest_layer(state | default_layer_state));

    if (layer != last_layer) {
        last_layer = layer;
        apply_layer_profile(layer);
    }

    return state;
}

#ifdef ENCODER_ENABLE
bool encoder_update_user(uint8_t index, bool clockwise) {
    (void)index;

    if (clockwise) {
        tap_code(MS_WHLU);
    } else {
        tap_code(MS_WHLD);
    }

    return false;
}
#endif

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    // Record physical key events independently of the currently assigned
    // keycode. This is the bridge between the keyboard matrix and all future
    // position-based LED effects (glow, ripple, heatmap, comet, ...).
    positional_rgb_handle_key_event(record);

    if (!record->event.pressed) {
        return true;
    }

    switch (keycode) {
        case RM_TOGG:
            return false;
        case SAFE_EEPROM_RESET:
            if (encoder_button_is_pressed()) {
                encoder_btn_consumed = true;
                // Reset both QMK's persistent settings and VIA's separate
                // dynamic-keymap/macro storage, then reload the keymap from flash.
                eeconfig_init();
                eeconfig_init_via();
                reset_keyboard();
            }
            return false;
        case SAFE_BOOT:
            if (encoder_button_is_pressed()) {
                encoder_btn_consumed = true;
                reset_keyboard();
            }
            return false;
        default:
            return true;
    }
}

bool rgb_matrix_indicators_advanced_user(uint8_t led_min, uint8_t led_max) {
    uint8_t layer = clamp_layer(last_layer);
    uint8_t effect = clamp_effect(rgb_cfg.layer_effect[layer]);
    hsv_t current_hsv = rgb_matrix_get_hsv();

    if (effect == LFX_REACTIVE_SIMPLE) {
        positional_rgb_render_reactive_glow(
            led_min,
            led_max,
            rgb_cfg.layer_hue_a[layer],
            rgb_cfg.layer_sat_a[layer],
            current_hsv.v,
            rgb_cfg.layer_speed[layer]
        );
        return false;
    }

    if (!is_dual_effect(effect)) {
        return false;
    }

    rgb_t color_a = layer_color(rgb_cfg.layer_hue_a[layer], rgb_cfg.layer_sat_a[layer], current_hsv.v);
    rgb_t color_b = layer_color(rgb_cfg.layer_hue_b[layer], rgb_cfg.layer_sat_b[layer], current_hsv.v);
    uint8_t speed = rgb_cfg.layer_speed[layer];
    uint32_t cycle_ms = animation_cycle_ms(speed);
    uint8_t phase = (uint8_t)(((timer_read32() % cycle_ms) * 255U) / cycle_ms);

    for (uint8_t i = led_min; i < led_max; i++) {
        uint8_t amount = 0;

        switch (effect) {
            case LFX_DUAL_GRADIENT:
                amount = (uint8_t)(((uint16_t)g_led_config.point[i].x * 255U) / 224U);
                break;

            case LFX_DUAL_BREATH:
                amount = sin8(phase);
                break;

            case LFX_DUAL_WAVE: {
                uint8_t x_phase = (uint8_t)(((uint16_t)g_led_config.point[i].x * 255U) / 224U);
                amount = triangle8((uint8_t)(phase + x_phase));
                break;
            }

            default:
                break;
        }

        rgb_t mixed = blend_rgb(color_a, color_b, amount);
        rgb_matrix_set_color(i, mixed.r, mixed.g, mixed.b);
    }

    return false;
}

static void via_rgb_ui_set_value(uint8_t *data) {
    uint8_t value_id = data[0];
    uint8_t *value_data = &data[1];

    switch (value_id) {
        case id_layer_color_a: {
            uint8_t index = value_data[0];
            if (index < LAYER_COUNT) {
                rgb_cfg.layer_hue_a[index] = value_data[1];
                rgb_cfg.layer_sat_a[index] = value_data[2];
                if (index == last_layer) {
                    apply_layer_profile(last_layer);
                }
            }
            break;
        }
        case id_layer_color_b: {
            uint8_t index = value_data[0];
            if (index < LAYER_COUNT) {
                rgb_cfg.layer_hue_b[index] = value_data[1];
                rgb_cfg.layer_sat_b[index] = value_data[2];
            }
            break;
        }
        case id_layer_effect: {
            uint8_t index = value_data[0];
            if (index < LAYER_COUNT) {
                rgb_cfg.layer_effect[index] = clamp_effect(value_data[1]);
                if (index == last_layer) {
                    apply_layer_profile(last_layer);
                }
            }
            break;
        }
        case id_layer_speed: {
            uint8_t index = value_data[0];
            if (index < LAYER_COUNT) {
                rgb_cfg.layer_speed[index] = value_data[1];
                if (index == last_layer) {
                    apply_layer_profile(last_layer);
                }
            }
            break;
        }
    }
}

static void via_rgb_ui_get_value(uint8_t *data) {
    uint8_t value_id = data[0];
    uint8_t *value_data = &data[1];

    switch (value_id) {
        case id_layer_color_a: {
            uint8_t index = value_data[0];
            if (index < LAYER_COUNT) {
                value_data[1] = rgb_cfg.layer_hue_a[index];
                value_data[2] = rgb_cfg.layer_sat_a[index];
            }
            break;
        }
        case id_layer_color_b: {
            uint8_t index = value_data[0];
            if (index < LAYER_COUNT) {
                value_data[1] = rgb_cfg.layer_hue_b[index];
                value_data[2] = rgb_cfg.layer_sat_b[index];
            }
            break;
        }
        case id_layer_effect: {
            uint8_t index = value_data[0];
            if (index < LAYER_COUNT) {
                value_data[1] = clamp_effect(rgb_cfg.layer_effect[index]);
            }
            break;
        }
        case id_layer_speed: {
            uint8_t index = value_data[0];
            if (index < LAYER_COUNT) {
                value_data[1] = rgb_cfg.layer_speed[index];
            }
            break;
        }
    }
}

void via_custom_value_command_kb(uint8_t *data, uint8_t length) {
    (void)length;

    uint8_t *command_id = &data[0];
    uint8_t *channel_id = &data[1];
    uint8_t *value_id_and_data = &data[2];

    if (*channel_id != id_custom_channel) {
        *command_id = id_unhandled;
        return;
    }

    switch (*command_id) {
        case id_custom_set_value:
            via_rgb_ui_set_value(value_id_and_data);
            break;
        case id_custom_get_value:
            via_rgb_ui_get_value(value_id_and_data);
            break;
        case id_custom_save:
            rgb_ui_save();
            break;
        default:
            *command_id = id_unhandled;
            break;
    }
}

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [_CREATIVE] = LAYOUT_6x4(
        KC_NO,          TO(0),      MO(4),      KC_BSPC,
        KC_S,           KC_C,       KC_V,       KC_Z,
        KC_B,           KC_N,       KC_X,       KC_LCTL,
        KC_LEFT,        KC_UP,      KC_RIGHT,   KC_NO,
        KC_A,           KC_DOWN,    KC_ENT,     KC_LSFT,
        KC_NO,          KC_LALT,    KC_SPACE,   KC_NO
    ),

    [_NUMPAD] = LAYOUT_6x4(
        KC_NO,          TO(0),          MO(4),          KC_BSPC,
        KC_NUM,         KC_PAST,        KC_PSLS,        KC_PMNS,
        KC_P7,          KC_P8,          KC_P9,          KC_PPLS,
        KC_P4,          KC_P5,          KC_P6,          KC_NO,
        KC_P1,          KC_P2,          KC_P3,          KC_PENT,
        KC_NO,          KC_P0,          KC_PDOT,        KC_NO
    ),

    [_NAV] = LAYOUT_6x4(
        KC_NO,                   TO(0),                MO(4),                    KC_NO,
        KC_NO,                   KC_NO,                KC_NO,                    KC_NO,
        KC_NO,                   KC_NO,                KC_NO,                    KC_LCTL,
        LALT(LCTL(KC_LEFT)),     KC_NO,                LALT(LCTL(KC_RGHT)),      KC_NO,
        LCTL(LGUI(KC_LEFT)),     KC_NO,                LCTL(LGUI(KC_RGHT)),      KC_LSFT,
        KC_NO,                   KC_LALT,              LCTL(LALT(KC_DEL)),       KC_NO
    ),

    [_GRAPHICS] = LAYOUT_6x4(
        KC_NO,          TO(0),      MO(4),      KC_ESC,
        KC_K,           KC_R,       KC_E,       MS_BTN1,
        KC_P,           KC_F,       KC_G,       KC_LCTL,
        KC_LEFT,        KC_UP,      KC_RIGHT,   KC_NO,
        KC_T,           KC_DOWN,    KC_ENT,     KC_LSFT,
        KC_NO,          KC_LALT,    KC_SPACE,   KC_NO
    ),

    [_SETTINGS] = LAYOUT_6x4(
        KC_NO,          TO(0),      MO(4),      RM_PREV,
        RM_SPDU,        RM_SPDD,    RM_HUEU,    RM_HUED,
        RM_VALU,        RM_VALD,    RM_NEXT,    KC_NO,
        RM_SATU,        RM_SATD,    KC_NO,      TO(0),
        TO(1),          TO(2),      TO(3),      TO(0),
        KC_NO,          KC_NO,      SAFE_EEPROM_RESET, SAFE_BOOT
    )
};
