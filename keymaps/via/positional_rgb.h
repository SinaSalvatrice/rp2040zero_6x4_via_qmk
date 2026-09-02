#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Generic key/LED position layer for RGB effects.
// Key positions and LED positions are intentionally independent: there is no
// Key N -> LED N relationship anywhere in this module.

#ifndef POSITIONAL_RGB_LED_COUNT
#    define POSITIONAL_RGB_LED_COUNT 30
#endif
#ifndef POSITIONAL_RGB_REACTIVE_RADIUS
#    define POSITIONAL_RGB_REACTIVE_RADIUS 110
#endif
#ifndef POSITIONAL_RGB_REACTIVE_FALLOFF
#    define POSITIONAL_RGB_REACTIVE_FALLOFF 2
#endif
#ifndef POSITIONAL_RGB_REACTIVE_BRIGHTNESS
#    define POSITIONAL_RGB_REACTIVE_BRIGHTNESS 255
#endif
#ifndef POSITIONAL_RGB_DECAY_MIN_MS
#    define POSITIONAL_RGB_DECAY_MIN_MS 180
#endif
#ifndef POSITIONAL_RGB_DECAY_MAX_MS
#    define POSITIONAL_RGB_DECAY_MAX_MS 900
#endif
#ifndef POSITIONAL_RGB_EVENT_HISTORY
#    define POSITIONAL_RGB_EVENT_HISTORY 16
#endif

typedef struct {
    int16_t x;
    int16_t y;
} positional_point_t;

typedef struct {
    bool     held;
    uint32_t pressed_at;
    uint32_t released_at;
} positional_key_state_t;

typedef struct {
    bool     valid;
    uint8_t  row;
    uint8_t  col;
    uint32_t pressed_at;
} positional_key_event_t;

// Current 6x4 keyboard geometry. One key pitch = 100 virtual units.
// These coordinates may be changed independently of the LED table.
static const positional_point_t positional_key_positions[MATRIX_ROWS][MATRIX_COLS] = {
    {{0, 0},   {100, 0},   {200, 0},   {300, 0}},
    {{0, 100}, {100, 100}, {200, 100}, {300, 100}},
    {{0, 200}, {100, 200}, {200, 200}, {300, 200}},
    {{0, 300}, {100, 300}, {200, 300}, {300, 300}},
    {{0, 400}, {100, 400}, {200, 400}, {300, 400}},
    {{0, 500}, {100, 500}, {200, 500}, {300, 500}},
};

// Current hardware: 6 rows, 5 pixels below each row = 30 pixels.
// The five LEDs span exactly the same width as the four keys:
// LED X = 0/75/150/225/300 while Key X = 0/100/200/300.
//
// IMPORTANT: this table follows physical LED IDs in wire order. If a strip is
// wired in reverse/serpentine order, only reorder coordinates in this table;
// key positions and effect code do not need to change.
static const positional_point_t positional_led_positions[POSITIONAL_RGB_LED_COUNT] = {
    {0, 0},   {75, 0},   {150, 0},   {225, 0},   {300, 0},
    {0, 100}, {75, 100}, {150, 100}, {225, 100}, {300, 100},
    {0, 200}, {75, 200}, {150, 200}, {225, 200}, {300, 200},
    {0, 300}, {75, 300}, {150, 300}, {225, 300}, {300, 300},
    {0, 400}, {75, 400}, {150, 400}, {225, 400}, {300, 400},
    {0, 500}, {75, 500}, {150, 500}, {225, 500}, {300, 500},
};

static positional_key_state_t positional_key_states[MATRIX_ROWS][MATRIX_COLS];
static positional_key_event_t positional_event_history[POSITIONAL_RGB_EVENT_HISTORY];
static uint8_t positional_event_head = 0;

static uint8_t positional_scale8(uint8_t value, uint8_t scale) {
    return (uint8_t)(((uint16_t)value * (uint16_t)scale + 127U) / 255U);
}

static uint8_t positional_soft_add8(uint8_t a, uint8_t b) {
    // Screen-style accumulation: several simultaneous keys add energy without
    // clipping abruptly at 255.
    return (uint8_t)(255U - (((uint16_t)(255U - a) * (uint16_t)(255U - b) + 127U) / 255U));
}

static uint32_t positional_isqrt32(uint32_t value) {
    // Integer square root; avoids floating point in the RGB render loop.
    uint32_t result = 0;
    uint32_t bit = 1UL << 30;

    while (bit > value) {
        bit >>= 2;
    }

    while (bit != 0) {
        if (value >= result + bit) {
            value -= result + bit;
            result = (result >> 1) + bit;
        } else {
            result >>= 1;
        }
        bit >>= 2;
    }

    return result;
}

static uint16_t positional_distance(positional_point_t a, positional_point_t b) {
    int32_t dx = (int32_t)a.x - (int32_t)b.x;
    int32_t dy = (int32_t)a.y - (int32_t)b.y;
    uint32_t squared = (uint32_t)(dx * dx + dy * dy);
    return (uint16_t)positional_isqrt32(squared);
}

// Accessors are deliberately part of the small positional API so future
// ripple/heatmap/comet effects can reuse the exact same geometry and event log.
static inline const positional_point_t *positional_rgb_key_position(uint8_t row, uint8_t col) {
    if (row >= MATRIX_ROWS || col >= MATRIX_COLS) {
        return NULL;
    }
    return &positional_key_positions[row][col];
}

static inline const positional_point_t *positional_rgb_led_position(uint8_t led) {
    if (led >= POSITIONAL_RGB_LED_COUNT) {
        return NULL;
    }
    return &positional_led_positions[led];
}

static inline const positional_key_event_t *positional_rgb_recent_event(uint8_t newest_index) {
    if (newest_index >= POSITIONAL_RGB_EVENT_HISTORY) {
        return NULL;
    }

    uint8_t slot = (uint8_t)((positional_event_head + POSITIONAL_RGB_EVENT_HISTORY - 1U - newest_index) % POSITIONAL_RGB_EVENT_HISTORY);
    return positional_event_history[slot].valid ? &positional_event_history[slot] : NULL;
}

static void positional_rgb_reset_state(void) {
    for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
        for (uint8_t col = 0; col < MATRIX_COLS; col++) {
            positional_key_states[row][col].held = false;
            positional_key_states[row][col].pressed_at = 0;
            positional_key_states[row][col].released_at = 0;
        }
    }

    for (uint8_t i = 0; i < POSITIONAL_RGB_EVENT_HISTORY; i++) {
        positional_event_history[i].valid = false;
        positional_event_history[i].row = 0;
        positional_event_history[i].col = 0;
        positional_event_history[i].pressed_at = 0;
    }
    positional_event_head = 0;
}

static void positional_rgb_handle_key_event(const keyrecord_t *record) {
    uint8_t row = record->event.key.row;
    uint8_t col = record->event.key.col;

    if (row >= MATRIX_ROWS || col >= MATRIX_COLS) {
        return;
    }

    uint32_t now = timer_read32();
    positional_key_state_t *state = &positional_key_states[row][col];

    if (record->event.pressed) {
        state->held = true;
        state->pressed_at = now;
        state->released_at = 0;

        positional_key_event_t *event = &positional_event_history[positional_event_head];
        event->valid = true;
        event->row = row;
        event->col = col;
        event->pressed_at = now;
        positional_event_head = (uint8_t)((positional_event_head + 1U) % POSITIONAL_RGB_EVENT_HISTORY);
    } else {
        state->held = false;
        state->released_at = now;
    }
}

static uint16_t positional_decay_ms(uint8_t speed) {
    uint16_t range = POSITIONAL_RGB_DECAY_MAX_MS - POSITIONAL_RGB_DECAY_MIN_MS;
    return (uint16_t)(POSITIONAL_RGB_DECAY_MAX_MS - (((uint32_t)range * speed) / 255U));
}

static uint8_t positional_key_strength(const positional_key_state_t *state, uint32_t now, uint8_t speed) {
    if (state->held) {
        return 255;
    }
    if (state->released_at == 0) {
        return 0;
    }

    uint16_t decay = positional_decay_ms(speed);
    uint32_t elapsed = now - state->released_at;
    if (elapsed >= decay) {
        return 0;
    }

    return (uint8_t)(255U - ((elapsed * 255U) / decay));
}

static uint8_t positional_spatial_strength(positional_point_t key, positional_point_t led) {
    uint16_t distance = positional_distance(key, led);
    if (distance >= POSITIONAL_RGB_REACTIVE_RADIUS) {
        return 0;
    }

    uint8_t strength = (uint8_t)(255U - (((uint32_t)distance * 255U) / POSITIONAL_RGB_REACTIVE_RADIUS));

    // Falloff 1 = linear. 2 = quadratic, 3 = cubic, etc.
    for (uint8_t i = 1; i < POSITIONAL_RGB_REACTIVE_FALLOFF; i++) {
        strength = positional_scale8(strength, strength);
    }

    return strength;
}

static void positional_rgb_render_reactive_glow(uint8_t led_min, uint8_t led_max, uint8_t hue, uint8_t sat, uint8_t global_value, uint8_t speed) {
    uint32_t now = timer_read32();
    uint8_t safe_led_max = led_max > POSITIONAL_RGB_LED_COUNT ? POSITIONAL_RGB_LED_COUNT : led_max;

    for (uint8_t led = led_min; led < safe_led_max; led++) {
        uint8_t combined = 0;
        positional_point_t led_position = positional_led_positions[led];

        for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
            for (uint8_t col = 0; col < MATRIX_COLS; col++) {
                uint8_t temporal = positional_key_strength(&positional_key_states[row][col], now, speed);
                if (temporal == 0) {
                    continue;
                }

                uint8_t spatial = positional_spatial_strength(positional_key_positions[row][col], led_position);
                if (spatial == 0) {
                    continue;
                }

                uint8_t contribution = positional_scale8(spatial, temporal);
                combined = positional_soft_add8(combined, contribution);
            }
        }

        uint8_t value = positional_scale8(combined, POSITIONAL_RGB_REACTIVE_BRIGHTNESS);
        value = positional_scale8(value, global_value);

        hsv_t hsv = {.h = hue, .s = sat, .v = value};
        rgb_t rgb = hsv_to_rgb(hsv);
        rgb_matrix_set_color(led, rgb.r, rgb.g, rgb.b);
    }
}
