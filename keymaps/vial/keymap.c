#include QMK_KEYBOARD_H
#include "via.h"
#include "eeconfig.h"

#define LAYER_COUNT 5

enum layer_names {
    _NUMPAD,
    _EDIT,
    _NAV,
    _INKSCAPE,
    _SETTINGS
};

enum via_rgb_ui_value {
    id_layer_color = 1
};

typedef struct {
    uint8_t layer_hue[LAYER_COUNT];
    uint8_t layer_sat[LAYER_COUNT];
} rgb_ui_config_t;

static rgb_ui_config_t rgb_cfg = {
    .layer_hue = {149, 64, 170, 155, 0},
    .layer_sat = {255, 255, 255, 255, 255}
};

static uint8_t last_layer = _NUMPAD;
static bool btn_released = true;
static uint16_t btn_tmr = 0;

static const uint8_t indicator_led_for_layer[LAYER_COUNT] = {0, 2, 7, 9, 4};

static void rgb_ui_save(void) {
    eeconfig_update_user_datablock(&rgb_cfg, 0, sizeof(rgb_cfg));
}

static void rgb_ui_load(void) {
    if (!eeconfig_is_user_datablock_valid()) {
        eeconfig_init_user_datablock();
        rgb_ui_save();
    } else {
        eeconfig_read_user_datablock(&rgb_cfg, 0, sizeof(rgb_cfg));
    }
}

static void apply_layer_color(uint8_t layer) {
    if (layer >= LAYER_COUNT) {
        layer = _NUMPAD;
    }
    hsv_t hsv = rgb_matrix_get_hsv();
    rgb_matrix_sethsv_noeeprom(rgb_cfg.layer_hue[layer], rgb_cfg.layer_sat[layer], hsv.v);
}

void keyboard_post_init_user(void) {
#ifdef ENCODER_BTN_PIN
    gpio_set_pin_input_high(ENCODER_BTN_PIN);
#endif
    rgb_ui_load();
    rgb_matrix_enable_noeeprom();
    last_layer = get_highest_layer(layer_state | default_layer_state);
    apply_layer_color(last_layer);
}

void matrix_scan_user(void) {
#ifdef ENCODER_BTN_PIN
    if (timer_elapsed(btn_tmr) >= 10) {
        bool pressed = !gpio_read_pin(ENCODER_BTN_PIN);
        if (pressed && btn_released) {
            btn_tmr = timer_read();
            if (last_layer == _INKSCAPE) {
                tap_code(KC_3);
            } else {
                rgb_matrix_toggle_noeeprom();
            }
        }
        btn_released = !pressed;
    }
#endif
}

layer_state_t layer_state_set_user(layer_state_t state) {
    uint8_t layer = get_highest_layer(state | default_layer_state);
    if (layer != last_layer) {
        last_layer = layer;
        apply_layer_color(layer);
    }
    return state;
}

bool rgb_matrix_indicators_advanced_user(uint8_t led_min, uint8_t led_max) {
    uint8_t layer = (last_layer < LAYER_COUNT) ? last_layer : _NUMPAD;
    uint8_t idx = indicator_led_for_layer[layer];
    if (idx >= led_min && idx < led_max) {
        rgb_matrix_set_color(idx, 255, 255, 255);
    }
    return false;
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
    } else {
        if (clockwise) {
            tap_code(MS_WHLU);
        } else {
            tap_code(MS_WHLD);
        }
    }
    return false;
}
#endif

static void via_rgb_ui_set_value(uint8_t *data) {
    uint8_t value_id = data[0];
    uint8_t *value_data = &data[1];

    if (value_id == id_layer_color) {
        uint8_t index = value_data[0];
        if (index < LAYER_COUNT) {
            rgb_cfg.layer_hue[index] = value_data[1];
            rgb_cfg.layer_sat[index] = value_data[2];
            if (index == last_layer) {
                apply_layer_color(last_layer);
            }
        }
    }
}

static void via_rgb_ui_get_value(uint8_t *data) {
    uint8_t value_id = data[0];
    uint8_t *value_data = &data[1];

    if (value_id == id_layer_color) {
        uint8_t index = value_data[0];
        if (index < LAYER_COUNT) {
            value_data[1] = rgb_cfg.layer_hue[index];
            value_data[2] = rgb_cfg.layer_sat[index];
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
        KC_NO,          MO(1),          MO(4),          KC_BSPC,
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
        KC_NO,          TO(0),      MO(4),      QK_BOOT,
        RM_SPDU,        RM_SPDD,    RM_HUEU,    RM_HUED,
        RM_VALU,        RM_VALD,    RM_NEXT,    RM_TOGG,
        RM_SATU,        RM_SATD,    KC_NO,      KC_NO,
        TO(1),          TO(2),      TO(3),      KC_NO,
        KC_NO,          KC_NO,      KC_NO,      KC_NO
    )
};
