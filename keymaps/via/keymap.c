#include QMK_KEYBOARD_H
#include "lib/lib8tion/lib8tion.h"
#include "timer.h"
#include "via.h"
#include "eeconfig.h"

#if defined(RGBLIGHT_LED_COUNT)
#    define LED_COUNT RGBLIGHT_LED_COUNT
#elif defined(RGBLED_NUM)
#    define LED_COUNT RGBLED_NUM
#else
#    define LED_COUNT 21
#endif

#define FRAME_MS              20
#define WANDER_V              55
#define WANDER_TRAIL_V        24
#define DOT_V                 80
#define DOT_HOLD_MS          250
#define DOT_STEP_PER_TICK      1
#define IND_HOLD_MS          280
#define IND_V                255

enum layer_names {
    _NUMPAD,
    _EDIT,
    _NAV,
    _INKSCAPE,
    _SETTINGS
};

enum custom_keycodes {
    RGB_UI_TOG = SAFE_RANGE,
    RGB_UI_WTOG,
    RGB_UI_HUI,
    RGB_UI_HUD,
    RGB_UI_SAI,
    RGB_UI_SAD,
    RGB_UI_VAI,
    RGB_UI_VAD,
    RGB_UI_WSPD_UP,
    RGB_UI_WSPD_DN
};

enum via_rgb_ui_value {
    id_layer_color = 1,
    id_base_brightness,
    id_animation_mode,
    id_animation_speed,
    id_rgb_enable
};

typedef struct {
    uint8_t  layer_hue[5];
    uint8_t  layer_sat[5];
    uint8_t  base_v_max;
    uint8_t  base_v_min;
    uint16_t wander_step_ms;
    uint8_t  rgb_mode;
    uint8_t  user_rgb_on;
} rgb_ui_config_t;

static rgb_ui_config_t rgb_cfg = {
    .layer_hue = {149, 64, 170, 155, 0},
    .layer_sat = {255, 255, 255, 255, 255},
    .base_v_max = 25,
    .base_v_min = 1,
    .wander_step_ms = 120,
    .rgb_mode = 1,
    .user_rgb_on = 1
};

static uint16_t t_frame = 0;
static uint16_t last_turn = 0;
static uint8_t enc_dot_pos = 0;
static uint8_t wander_pos = 0;
static uint16_t wander_tmr = 0;
static uint8_t current_hue = 149;
static uint8_t current_sat = 255;
static uint8_t last_layer = 0;
static uint16_t ind_tmr = 0;
static bool ind_active = false;
static bool btn_released = true;
static uint16_t btn_tmr = 0;

static const uint8_t indicator_led_for_layer[5] = {0, 2, 7, 9, 4};

static uint8_t dither_scale_sin8(uint16_t now_div, uint8_t vmax) {
    uint8_t s = sin8(now_div);
    uint16_t v16 = (uint16_t)s * vmax;
    uint8_t v = v16 >> 8;
    uint8_t frac = v16 & 0xFF;
    uint16_t n = timer_read();
    uint8_t r = (uint8_t)(n ^ (n >> 8));
    if (r < frac && v < vmax) v++;
    return v;
}

static inline void set_led_hsv(uint8_t idx, uint8_t h, uint8_t s, uint8_t v) {
    if (idx < LED_COUNT) rgblight_sethsv_at(h, s, v, idx);
}

static void clear_all_leds(void) {
    for (uint8_t i = 0; i < LED_COUNT; i++) set_led_hsv(i, 0, 0, 0);
}

static void apply_layer_color(uint8_t layer) {
    if (layer >= 5) layer = 0;
    current_hue = rgb_cfg.layer_hue[layer];
    current_sat = rgb_cfg.layer_sat[layer];
}

static void rgb_ui_save(void) {
    eeconfig_update_user_datablock(&rgb_cfg, 0, sizeof(rgb_cfg));
}

static void rgb_ui_load(void) {
    if (!eeconfig_is_user_datablock_valid()) {
        eeconfig_init_user_datablock();
        rgb_ui_save();
    } else {
        eeconfig_read_user_datablock(&rgb_cfg, 0, sizeof(rgb_cfg));
        if (rgb_cfg.wander_step_ms < 20 || rgb_cfg.wander_step_ms > 1000) rgb_cfg.wander_step_ms = 120;
        if (rgb_cfg.rgb_mode > 2) rgb_cfg.rgb_mode = 1;
        if (rgb_cfg.base_v_max > 100) rgb_cfg.base_v_max = 25;
        if (rgb_cfg.base_v_min > rgb_cfg.base_v_max) rgb_cfg.base_v_min = 1;
    }
}

static void render_frame(void) {
    if (!rgb_cfg.user_rgb_on) {
        clear_all_leds();
        return;
    }

    uint16_t now = timer_read();

    if (rgb_cfg.rgb_mode == 1 || rgb_cfg.rgb_mode == 2) {
        uint8_t base_v = rgb_cfg.base_v_min;
        if (rgb_cfg.base_v_max > rgb_cfg.base_v_min) {
            uint8_t span = rgb_cfg.base_v_max - rgb_cfg.base_v_min;
            base_v = dither_scale_sin8(now / 14, span) + rgb_cfg.base_v_min;
        }
        for (uint8_t i = 0; i < LED_COUNT; i++) set_led_hsv(i, current_hue, current_sat, base_v);
    } else {
        clear_all_leds();
    }

    if (rgb_cfg.rgb_mode != 2) {
        uint8_t w_v = dither_scale_sin8(now / 10, WANDER_V);
        uint8_t wp = (wander_pos >= LED_COUNT) ? 0 : wander_pos;
        uint8_t left = (wp == 0) ? (LED_COUNT - 1) : (wp - 1);
        uint8_t right = (wp + 1) % LED_COUNT;
        uint8_t trail_v = dither_scale_sin8(now / 12, WANDER_TRAIL_V);

        if (last_layer == _NUMPAD) {
            uint8_t rainbow_base = (now / 8) & 0xFF;
            uint8_t step = (uint8_t)(256 / LED_COUNT);
            set_led_hsv(left, rainbow_base + left * step, 255, trail_v);
            set_led_hsv(wp, rainbow_base + wp * step, 255, w_v);
            set_led_hsv(right, rainbow_base + right * step, 255, trail_v);
        } else {
            set_led_hsv(left, current_hue, current_sat, trail_v);
            set_led_hsv(wp, current_hue, current_sat, w_v);
            set_led_hsv(right, current_hue, current_sat, trail_v);
        }
    }

    if (ind_active && timer_elapsed(ind_tmr) < IND_HOLD_MS) {
        uint8_t idx = indicator_led_for_layer[(last_layer < 5) ? last_layer : 0];
        set_led_hsv(idx, current_hue, current_sat, IND_V);
    } else {
        ind_active = false;
    }

    if (timer_elapsed(last_turn) < DOT_HOLD_MS) {
        uint8_t dot_v = dither_scale_sin8(now / 6, DOT_V);
        set_led_hsv((enc_dot_pos >= LED_COUNT) ? 0 : enc_dot_pos, current_hue, current_sat, dot_v);
    }
}

void keyboard_post_init_user(void) {
#ifdef ENCODER_BTN_PIN
    gpio_set_pin_input_high(ENCODER_BTN_PIN);
#endif
    rgblight_enable_noeeprom();
    rgb_ui_load();
    t_frame = timer_read();
    wander_tmr = timer_read();
    last_layer = get_highest_layer(layer_state | default_layer_state);
    apply_layer_color(last_layer);
    ind_active = true;
    ind_tmr = timer_read();
    render_frame();
}

void matrix_scan_user(void) {
    if (timer_elapsed(t_frame) >= FRAME_MS) {
        t_frame = timer_read();
        if (timer_elapsed(wander_tmr) >= rgb_cfg.wander_step_ms) {
            wander_tmr = timer_read();
            wander_pos = (wander_pos + 1) % LED_COUNT;
        }
        render_frame();
    }
#ifdef ENCODER_BTN_PIN
    if (timer_elapsed(btn_tmr) >= 10) {
        bool pressed = !gpio_read_pin(ENCODER_BTN_PIN);
        if (pressed && btn_released) {
            btn_tmr = timer_read();
            if (last_layer == _INKSCAPE) {
                tap_code(KC_3);
            } else {
                rgb_cfg.user_rgb_on = !rgb_cfg.user_rgb_on;
                if (!rgb_cfg.user_rgb_on) clear_all_leds();
                else {
                    ind_active = true;
                    ind_tmr = timer_read();
                    render_frame();
                }
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
        ind_active = true;
        ind_tmr = timer_read();
    }
    render_frame();
    return state;
}

#ifdef ENCODER_ENABLE
bool encoder_update_user(uint8_t index, bool clockwise) {
    (void)index;
    if (clockwise) {
        enc_dot_pos = (enc_dot_pos + DOT_STEP_PER_TICK) % LED_COUNT;
        if (last_layer == _INKSCAPE) tap_code16(KC_PLUS);
        else tap_code(MS_WHLU);
    } else {
        enc_dot_pos = (enc_dot_pos + LED_COUNT - DOT_STEP_PER_TICK) % LED_COUNT;
        if (last_layer == _INKSCAPE) tap_code(KC_MINS);
        else tap_code(MS_WHLD);
    }
    last_turn = timer_read();
    render_frame();
    return false;
}
#endif

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    if (!record->event.pressed) return true;
    switch (keycode) {
        case RGB_UI_TOG:
            rgb_cfg.user_rgb_on = !rgb_cfg.user_rgb_on;
            render_frame();
            return false;
        case RGB_UI_WTOG:
            rgb_cfg.rgb_mode = (rgb_cfg.rgb_mode + 1) % 3;
            render_frame();
            return false;
        case RGB_UI_HUI:
            rgb_cfg.layer_hue[last_layer < 5 ? last_layer : 0] += 8;
            apply_layer_color(last_layer);
            render_frame();
            return false;
        case RGB_UI_HUD:
            rgb_cfg.layer_hue[last_layer < 5 ? last_layer : 0] -= 8;
            apply_layer_color(last_layer);
            render_frame();
            return false;
        case RGB_UI_SAI: {
            uint8_t i = last_layer < 5 ? last_layer : 0;
            rgb_cfg.layer_sat[i] = rgb_cfg.layer_sat[i] <= 247 ? rgb_cfg.layer_sat[i] + 8 : 255;
            apply_layer_color(last_layer);
            render_frame();
            return false;
        }
        case RGB_UI_SAD: {
            uint8_t i = last_layer < 5 ? last_layer : 0;
            rgb_cfg.layer_sat[i] = rgb_cfg.layer_sat[i] >= 8 ? rgb_cfg.layer_sat[i] - 8 : 0;
            apply_layer_color(last_layer);
            render_frame();
            return false;
        }
        case RGB_UI_VAI:
            if (rgb_cfg.base_v_max < 100) rgb_cfg.base_v_max += 2;
            render_frame();
            return false;
        case RGB_UI_VAD:
            if (rgb_cfg.base_v_max > 2) rgb_cfg.base_v_max -= 2;
            render_frame();
            return false;
        case RGB_UI_WSPD_UP:
            if (rgb_cfg.wander_step_ms > 20) rgb_cfg.wander_step_ms -= 10;
            render_frame();
            return false;
        case RGB_UI_WSPD_DN:
            if (rgb_cfg.wander_step_ms < 1000) rgb_cfg.wander_step_ms += 10;
            render_frame();
            return false;
    }
    return true;
}

static void via_rgb_ui_set_value(uint8_t *data) {
    uint8_t value_id = data[0];
    uint8_t *value_data = &data[1];
    switch (value_id) {
        case id_layer_color: {
            uint8_t index = value_data[0];
            if (index < 5) {
                rgb_cfg.layer_hue[index] = value_data[1];
                rgb_cfg.layer_sat[index] = value_data[2];
                if (index == last_layer) apply_layer_color(last_layer);
            }
            break;
        }
        case id_base_brightness:
            rgb_cfg.base_v_max = value_data[0] > 100 ? 100 : value_data[0];
            break;
        case id_animation_mode:
            rgb_cfg.rgb_mode = value_data[0] > 2 ? 2 : value_data[0];
            break;
        case id_animation_speed: {
            uint16_t speed = ((uint16_t)value_data[0] << 8) | value_data[1];
            if (speed < 20) speed = 20;
            if (speed > 1000) speed = 1000;
            rgb_cfg.wander_step_ms = speed;
            break;
        }
        case id_rgb_enable:
            rgb_cfg.user_rgb_on = value_data[0] ? 1 : 0;
            break;
    }
    render_frame();
}

static void via_rgb_ui_get_value(uint8_t *data) {
    uint8_t value_id = data[0];
    uint8_t *value_data = &data[1];
    switch (value_id) {
        case id_layer_color: {
            uint8_t index = value_data[0];
            if (index < 5) {
                value_data[1] = rgb_cfg.layer_hue[index];
                value_data[2] = rgb_cfg.layer_sat[index];
            }
            break;
        }
        case id_base_brightness:
            value_data[0] = rgb_cfg.base_v_max;
            break;
        case id_animation_mode:
            value_data[0] = rgb_cfg.rgb_mode;
            break;
        case id_animation_speed:
            value_data[0] = rgb_cfg.wander_step_ms >> 8;
            value_data[1] = rgb_cfg.wander_step_ms & 0xFF;
            break;
        case id_rgb_enable:
            value_data[0] = rgb_cfg.user_rgb_on;
            break;
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
        KC_NO,          TO(0),                  MO(4),              QK_BOOT,
        RGB_UI_WSPD_UP, RGB_UI_WSPD_DN,         RGB_UI_HUI,         RGB_UI_HUD,
        RGB_UI_VAI,     RGB_UI_VAD,             RGB_UI_WTOG,        RGB_UI_TOG,
        RGB_UI_SAI,     RGB_UI_SAD,             KC_NO,              KC_NO,
        TO(1),          TO(2),                  TO(3),              KC_NO,
        KC_NO,          KC_NO,                  KC_NO,              KC_NO
    )
};
