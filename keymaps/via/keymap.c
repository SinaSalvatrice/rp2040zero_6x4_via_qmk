#include QMK_KEYBOARD_H
#include "eeconfig.h"
#include "timer.h"
#include "via.h"

#define LAYER_COUNT 5
#define LAYER_EFFECT_COUNT 7

enum layer_names {
    _NUMPAD,
    _EDIT,
    _NAV,
    _INKSCAPE,
    _SETTINGS
};

enum custom_keycodes {
    SAFE_BOOT = SAFE_RANGE
};

enum via_rgb_ui_value {
    id_layer_color = 1,
    id_layer_effect
};

typedef struct {
    uint8_t layer_hue[LAYER_COUNT];
    uint8_t layer_sat[LAYER_COUNT];
    uint8_t layer_effect[LAYER_COUNT];
} rgb_ui_config_t;

static const uint8_t layer_effect_modes[LAYER_EFFECT_COUNT] = {
    RGB_MATRIX_SOLID_COLOR,
    RGB_MATRIX_BREATHING,
    RGB_MATRIX_CYCLE_LEFT_RIGHT,
    RGB_MATRIX_CYCLE_UP_DOWN,
    RGB_MATRIX_RAINBOW_MOVING_CHEVRON,
    RGB_MATRIX_SPLASH,
    RGB_MATRIX_RAINDROPS
};

static rgb_ui_config_t rgb_cfg = {
    .layer_hue    = {149, 64, 170, 155, 0},
    .layer_sat    = {255, 255, 255, 255, 255},
    .layer_effect = {0, 1, 2, 4, 6}
};

static uint8_t last_layer = _NUMPAD;
static bool encoder_btn_pressed = false;
static bool encoder_btn_consumed = false;
static uint16_t encoder_btn_tmr = 0;

static uint8_t clamp_layer(uint8_t layer) {
    return layer < LAYER_COUNT ? layer : _NUMPAD;
}

static uint8_t clamp_effect(uint8_t effect) {
    return effect < LAYER_EFFECT_COUNT ? effect : 0;
}

static bool encoder_button_is_pressed(void) {
#ifdef ENCODER_BTN_PIN
    return !gpio_read_pin(ENCODER_BTN_PIN);
#else
    return false;
#endif
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

static void apply_layer_profile(uint8_t layer) {
    uint8_t safe_layer = clamp_layer(layer);
    uint8_t effect = clamp_effect(rgb_cfg.layer_effect[safe_layer]);
    hsv_t hsv = rgb_matrix_get_hsv();

    rgb_matrix_mode_noeeprom(layer_effect_modes[effect]);
    rgb_matrix_sethsv_noeeprom(rgb_cfg.layer_hue[safe_layer], rgb_cfg.layer_sat[safe_layer], hsv.v);
}

static void handle_encoder_button_tap(void) {
    if (last_layer == _INKSCAPE) {
        tap_code(KC_3);
    } else {
        rgb_matrix_toggle_noeeprom();
    }
}

void keyboard_post_init_user(void) {
#ifdef ENCODER_BTN_PIN
    gpio_set_pin_input_high(ENCODER_BTN_PIN);
#endif

    rgb_ui_load();
    rgb_matrix_enable_noeeprom();
    last_layer = clamp_layer(get_highest_layer(layer_state | default_layer_state));
    apply_layer_profile(last_layer);
}

void matrix_scan_user(void) {
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

    if (last_layer == _INKSCAPE) {
        if (clockwise) {
            tap_code16(KC_PLUS);
        } else {
            tap_code(KC_MINS);
        }
    } else if (clockwise) {
        tap_code(MS_WHLU);
    } else {
        tap_code(MS_WHLD);
    }

    return false;
}
#endif

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    if (!record->event.pressed) {
        return true;
    }

    switch (keycode) {
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

static void via_rgb_ui_set_value(uint8_t *data) {
    uint8_t value_id = data[0];
    uint8_t *value_data = &data[1];

    switch (value_id) {
        case id_layer_color: {
            uint8_t index = value_data[0];
            if (index < LAYER_COUNT) {
                rgb_cfg.layer_hue[index] = value_data[1];
                rgb_cfg.layer_sat[index] = value_data[2];
                if (index == last_layer) {
                    apply_layer_profile(last_layer);
                }
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
    }
}

static void via_rgb_ui_get_value(uint8_t *data) {
    uint8_t value_id = data[0];
    uint8_t *value_data = &data[1];

    switch (value_id) {
        case id_layer_color: {
            uint8_t index = value_data[0];
            if (index < LAYER_COUNT) {
                value_data[1] = rgb_cfg.layer_hue[index];
                value_data[2] = rgb_cfg.layer_sat[index];
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
    [_NUMPAD] = LAYOUT_6x4(
        KC_NO,          TO(0),          MO(4),          KC_BSPC,
        KC_NUM,         KC_PAST,        KC_PSLS,        KC_PMNS,
        KC_P7,          KC_P8,          KC_P9,          KC_PPLS,
        KC_P4,          KC_P5,          KC_P6,          KC_NO,
        KC_P1,          KC_P2,          KC_P3,          KC_PENT,
        KC_NO,          KC_P0,          KC_PDOT,        KC_NO
    ),

    [_EDIT] = LAYOUT_6x4(
        KC_NO,               TO(0),                MO(4),                KC_BSPC,
        KC_NO,               KC_NO,                LCTL(KC_V),           LCTL(KC_A),
        LCTL(KC_Z),          S(KC_HOME),           LCTL(KC_R),           LCTL(KC_C),
        S(KC_LEFT),          LCTL(KC_S),           S(KC_RGHT),           KC_NO,
        LCTL(LSFT(KC_LEFT)), S(KC_END),            LCTL(LSFT(KC_RGHT)),  KC_PENT,
        KC_NO,               KC_SPACE,             LCTL(KC_X),           KC_NO
    ),

    [_NAV] = LAYOUT_6x4(
        KC_NO,                   TO(0),                MO(4),                    KC_NO,
        KC_NO,                   KC_NO,                KC_NO,                    KC_NO,
        KC_NO,                   KC_NO,                KC_NO,                    KC_NO,
        LALT(LCTL(KC_LEFT)),     KC_NO,                LALT(LCTL(KC_RGHT)),      KC_NO,
        LCTL(LGUI(KC_LEFT)),     KC_NO,                LCTL(LGUI(KC_RGHT)),      KC_NO,
        KC_NO,                   KC_NO,                LCTL(LALT(KC_DEL)),       KC_NO
    ),

    [_INKSCAPE] = LAYOUT_6x4(
        KC_NO,                   TO(0),                MO(4),                    LCTL(KC_Z),
        KC_LCTL,                 KC_LSFT,              KC_NO,                    MS_BTN1,
        LCTL(KC_S),              LCTL(KC_C),           LCTL(KC_V),               LCTL(KC_K),
        KC_LEFT,                 KC_UP,                KC_RIGHT,                 LCTL(KC_K),
        KC_S,                    KC_DOWN,              KC_N,                     LCTL(KC_Y),
        KC_NO,                   KC_NO,                KC_B,                     LCTL(KC_Y)
    ),

    [_SETTINGS] = LAYOUT_6x4(
        KC_NO,          TO(0),      MO(4),      RM_PREV,
        RM_SPDU,        RM_SPDD,    RM_HUEU,    RM_HUED,
        RM_VALU,        RM_VALD,    RM_NEXT,    RM_TOGG,
        RM_SATU,        RM_SATD,    KC_NO,      KC_NO,
        TO(1),          TO(2),      TO(3),      KC_NO,
        KC_NO,          KC_NO,      KC_NO,      SAFE_BOOT
    )
};
