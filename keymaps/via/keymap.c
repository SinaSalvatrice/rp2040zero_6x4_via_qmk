#include QMK_KEYBOARD_H
#include "lib/lib8tion/lib8tion.h"
#include "timer.h"

#if defined(RGBLIGHT_LED_COUNT)
#    define LED_COUNT RGBLIGHT_LED_COUNT
#elif defined(RGBLED_NUM)
#    define LED_COUNT RGBLED_NUM
#else
#    define LED_COUNT 10
#endif

#define STARTUP_MS             0
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
    _MAKRO,
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

static uint8_t  base_v_max      = 25;
static uint8_t  base_v_min      = 1;
static uint16_t wander_step_ms  = 120;
static uint8_t  current_sat     = 255;

static uint16_t t_frame         = 0;
static uint16_t last_turn       = 0;
static uint8_t  enc_dot_pos     = 0;
static uint8_t  wander_pos      = 0;
static uint16_t wander_tmr      = 0;
static uint8_t  current_hue     = 128;
static uint8_t  last_layer      = 0;
static uint16_t ind_tmr         = 0;
static bool     ind_active      = false;
static uint8_t  rgb_mode        = 0;
static bool     user_rgb_on     = true;

static uint8_t rotate_layer(uint8_t current_layer, bool clockwise) {
    uint8_t layer = (current_layer > _MAKRO) ? _NUMPAD : current_layer;

    if (clockwise) {
        layer = (layer >= _MAKRO) ? _NUMPAD : (layer + 1);
    } else {
        layer = (layer == _NUMPAD) ? _MAKRO : (layer - 1);
    }

    return layer;
}

static uint8_t hue_for_layer(uint8_t layer) {
    switch (layer) {
        case _NUMPAD:   return 149;
        case _EDIT:     return 64;
        case _NAV:      return 170;
        case _MAKRO:    return 213;
        case _SETTINGS: return 0;
        default:        return 149;
    }
}

static const uint8_t indicator_led_for_layer[5] = { 0, 2, 7, 9, 4 };

static uint8_t dither_scale_sin8(uint16_t now_div, uint8_t vmax) {
    uint8_t s = sin8(now_div);
    uint16_t v16 = (uint16_t)s * vmax;
    uint8_t v = v16 >> 8;
    uint8_t frac = v16 & 0xFF;

    uint16_t n = timer_read();
    uint8_t r = (uint8_t)(n ^ (n >> 8));

    if (r < frac && v < vmax) {
        v++;
    }
    return v;
}

static inline void set_led_hsv(uint8_t idx, uint8_t h, uint8_t s, uint8_t v) {
    if (idx >= LED_COUNT) {
        return;
    }
    rgblight_sethsv_at(h, s, v, idx);
}

static void clear_all_leds(void) {
    for (uint8_t i = 0; i < LED_COUNT; i++) {
        set_led_hsv(i, 0, 0, 0);
    }
}

static void render_frame(void) {
    if (!user_rgb_on) {
        clear_all_leds();
        return;
    }

    uint16_t now = timer_read();

    if (rgb_mode == 2) {
        uint8_t base_v;
        if (base_v_max > base_v_min) {
            uint8_t span = base_v_max - base_v_min;
            base_v = dither_scale_sin8(now / 14, span) + base_v_min;
        } else {
            base_v = base_v_min;
        }

        for (uint8_t i = 0; i < LED_COUNT; i++) {
            set_led_hsv(i, current_hue, current_sat, base_v);
        }
    } else if (rgb_mode == 1) {
        uint8_t base_v;
        if (base_v_max > base_v_min) {
            uint8_t span = base_v_max - base_v_min;
            base_v = dither_scale_sin8(now / 14, span) + base_v_min;
        } else {
            base_v = base_v_min;
        }

        for (uint8_t i = 0; i < LED_COUNT; i++) {
            set_led_hsv(i, current_hue, current_sat, base_v);
        }
    } else {
        for (uint8_t i = 0; i < LED_COUNT; i++) {
            set_led_hsv(i, 0, 0, 0);
        }
    }

    if (rgb_mode != 2) {
        uint8_t w_v = dither_scale_sin8(now / 10, WANDER_V);
        uint8_t wp = (wander_pos >= LED_COUNT) ? 0 : wander_pos;
        uint8_t left = (wp == 0) ? (LED_COUNT - 1) : (wp - 1);
        uint8_t right = (wp + 1) % LED_COUNT;
        uint8_t trail_v = dither_scale_sin8(now / 12, WANDER_TRAIL_V);

        if (last_layer == _NUMPAD) {
            uint8_t rainbow_base = (now / 8) & 0xFF;
            uint8_t step = (uint8_t)(256 / LED_COUNT);
            set_led_hsv(left, rainbow_base + (left * step), 255, trail_v);
            set_led_hsv(wp, rainbow_base + (wp * step), 255, w_v);
            set_led_hsv(right, rainbow_base + (right * step), 255, trail_v);
        } else {
            set_led_hsv(left, current_hue, current_sat, trail_v);
            set_led_hsv(wp, current_hue, current_sat, w_v);
            set_led_hsv(right, current_hue, current_sat, trail_v);
        }
    }

    if (ind_active && timer_elapsed(ind_tmr) < IND_HOLD_MS) {
        uint8_t layer = last_layer;
        uint8_t idx = indicator_led_for_layer[(layer < 5) ? layer : 0];
        set_led_hsv(idx, current_hue, current_sat, IND_V);
    } else {
        ind_active = false;
    }

    if (timer_elapsed(last_turn) < DOT_HOLD_MS) {
        uint8_t dot_v = dither_scale_sin8(now / 6, DOT_V);
        uint8_t dp = (enc_dot_pos >= LED_COUNT) ? 0 : enc_dot_pos;
        set_led_hsv(dp, current_hue, current_sat, dot_v);
    }
}

void keyboard_post_init_user(void) {
#ifdef ENCODER_BTN_PIN
    setPinInputHigh(ENCODER_BTN_PIN);
#endif
    t_frame = timer_read();
    wander_tmr = timer_read();

    uint8_t layer = get_highest_layer(layer_state | default_layer_state);
    last_layer = layer;
    current_hue = hue_for_layer(layer);
    ind_active = true;
    ind_tmr = timer_read();
    rgb_mode = 0;
    user_rgb_on = true;
    render_frame();
}

void matrix_scan_user(void) {
    if (timer_elapsed(t_frame) >= FRAME_MS) {
        t_frame = timer_read();
        if (timer_elapsed(wander_tmr) >= wander_step_ms) {
            wander_tmr = timer_read();
            wander_pos = (wander_pos + 1) % LED_COUNT;
        }
        render_frame();
    }

}

static bool encoder_button_is_held(void) {
    return layer_state_is(_SETTINGS);
}

layer_state_t layer_state_set_user(layer_state_t state) {
    uint8_t layer = get_highest_layer(state | default_layer_state);
    current_hue = hue_for_layer(layer);

    if (layer != last_layer) {
        last_layer = layer;
        ind_active = true;
        ind_tmr = timer_read();
    }

    render_frame();
    return state;
}

#ifdef ENCODER_ENABLE
bool encoder_update_user(uint8_t index, bool clockwise) {
    (void)index;

    if (encoder_button_is_held()) {
        uint8_t current_layer = get_highest_layer(layer_state | default_layer_state);
        uint8_t next_layer = rotate_layer(current_layer, clockwise);

        layer_move(next_layer);

    layer_on(_SETTINGS);

        last_turn = timer_read();
        render_frame();
        return false;
    }

    if (clockwise) {
        enc_dot_pos = (enc_dot_pos + DOT_STEP_PER_TICK) % LED_COUNT;
    } else {
        enc_dot_pos = (enc_dot_pos + LED_COUNT - (DOT_STEP_PER_TICK % LED_COUNT)) % LED_COUNT;
    }

    last_turn = timer_read();
    render_frame();
    return true;
}
#endif

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    if (!record->event.pressed) {
        return true;
    }

    switch (keycode) {
        case RGB_UI_TOG:
            user_rgb_on = !user_rgb_on;
            if (!user_rgb_on) {
                clear_all_leds();
            } else {
                ind_active = true;
                ind_tmr = timer_read();
                render_frame();
            }
            return false;

        case RGB_UI_WTOG:
            rgb_mode = (rgb_mode + 1) % 3;
            ind_active = true;
            ind_tmr = timer_read();
            render_frame();
            return false;

        case RGB_UI_HUI:
            current_hue += 8;
            render_frame();
            return false;

        case RGB_UI_HUD:
            current_hue -= 8;
            render_frame();
            return false;

        case RGB_UI_SAI:
            current_sat = (current_sat <= 247) ? current_sat + 8 : 255;
            render_frame();
            return false;

        case RGB_UI_SAD:
            current_sat = (current_sat >= 8) ? current_sat - 8 : 0;
            render_frame();
            return false;

        case RGB_UI_VAI:
            if (base_v_max < 100) {
                base_v_max += 2;
            }
            render_frame();
            return false;

        case RGB_UI_VAD:
            if (base_v_max > 2) {
                base_v_max -= 2;
            }
            render_frame();
            return false;

        case RGB_UI_WSPD_UP:
            if (wander_step_ms > 20) {
                wander_step_ms -= 10;
            }
            render_frame();
            return false;

        case RGB_UI_WSPD_DN:
            if (wander_step_ms < 1000) {
                wander_step_ms += 10;
            }
            render_frame();
            return false;
    }

    return true;
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
        KC_NO,               KC_NO,                KC_NO,                LCTL(KC_A),
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

    [_MAKRO] = LAYOUT_6x4(
        KC_NO,  TO(0),  MO(4),  KC_NO,
        KC_NO,  KC_NO,  KC_NO,  KC_NO,
        KC_F14, KC_F15, KC_F16, KC_NO,
        KC_F17, KC_F18, KC_F19, KC_NO,
        KC_F20, KC_F21, KC_F22, KC_NO,
        KC_NO,  KC_NO,  KC_NO,  KC_NO
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
