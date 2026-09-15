#include QMK_KEYBOARD_H

#include "dynamic_keymap.h"
#include "eeprom.h"
#include "sendstring_german.h"
#include "timer.h"
#include "via.h"

#define TAP_DANCE_VIA_CHANNEL 6
#define TAP_DANCE_VALUE_SINGLE 2
#define TAP_DANCE_VALUE_DOUBLE 3

#define TAP_DANCE_CONFIG_MAGIC 0x54444B31UL
#define TAP_DANCE_CONFIG_RESERVED_SIZE 52U
#define TAP_DANCE_CONFIG_EEPROM_ADDR (DYNAMIC_KEYMAP_EEPROM_MAX_ADDR + 1U)
#define TAP_DANCE_OLD_CONFIG_EEPROM_ADDR 4064U
#define TAP_DANCE_BASE_LAYER 0

#ifndef TAPPING_TERM
#    define TAPPING_TERM 200
#endif

enum legacy_tap_dance_action_id {
    LEGACY_TD_ACTION_NONE = 0,
    LEGACY_TD_ACTION_SCREENSHOT,
    LEGACY_TD_ACTION_CLIPBOARD,
    LEGACY_TD_ACTION_TASK_MANAGER,
    LEGACY_TD_ACTION_EXPLORER,
    LEGACY_TD_ACTION_RUN,
    LEGACY_TD_ACTION_CLOSE_WINDOW,
    LEGACY_TD_ACTION_DESKTOP,
    LEGACY_TD_ACTION_SCREEN_RECORD,
    LEGACY_TD_ACTION_CIRCUITCURIOS_REPO,
    LEGACY_TD_ACTION_CUSTOM_1,
    LEGACY_TD_ACTION_CUSTOM_2,
    LEGACY_TD_ACTION_CUSTOM_3,
    LEGACY_TD_ACTION_CUSTOM_4,
    LEGACY_TD_ACTION_COUNT
};

typedef struct {
    uint8_t action[MATRIX_ROWS][MATRIX_COLS];
    uint16_t custom_action[4];
} legacy_tap_dance_editor_storage_t;

typedef struct {
    uint32_t magic;
    uint16_t double_keycode[MATRIX_ROWS][MATRIX_COLS];
} tap_dance_editor_storage_t;

typedef enum {
    TD_POSITION_IDLE = 0,
    TD_POSITION_WAIT_SECOND,
    TD_POSITION_SINGLE_HELD,
    TD_POSITION_DOUBLE_HELD,
} tap_dance_position_stage_t;

typedef struct {
    tap_dance_position_stage_t stage;
    uint8_t row;
    uint8_t col;
    bool pressed;
    uint16_t timer;
    uint16_t single_keycode;
    uint16_t double_keycode;
} tap_dance_position_state_t;

STATIC_ASSERT(sizeof(legacy_tap_dance_editor_storage_t) == 32, "Legacy tap dance storage must stay 32 bytes");
STATIC_ASSERT(sizeof(tap_dance_editor_storage_t) == TAP_DANCE_CONFIG_RESERVED_SIZE, "Tap dance editor EEPROM reservation must stay exactly 52 bytes");

void via_custom_value_command_kb(uint8_t *data, uint8_t length);

static tap_dance_editor_storage_t tap_dance_config;
static tap_dance_position_state_t tap_dance_state = {0};

static bool tap_dance_position_valid(uint8_t row, uint8_t col) {
    return row < MATRIX_ROWS && col < MATRIX_COLS;
}

static bool tap_dance_position_editable(uint8_t row, uint8_t col) {
    // R0/C0 is the encoder position. R5 is reserved for the firmware-protected
    // modifier/layer tap dances and intentionally stays out of this editor.
    return tap_dance_position_valid(row, col) && row < (MATRIX_ROWS - 1) && !(row == 0 && col == 0);
}

static uint16_t tap_dance_resolve_keycode(uint16_t keycode) {
    // QK_KB_0..7 are the friendly VIA custom shortcuts. Resolve them to the
    // actual Windows chords because register_code16() does not call
    // process_record_kb() for nested tap-dance actions.
    switch (keycode) {
        case QK_KB_0:
            return LGUI(LSFT(KC_S));
        case QK_KB_1:
            return LGUI(KC_V);
        case QK_KB_2:
            return LCTL(LSFT(KC_ESC));
        case QK_KB_3:
            return LGUI(KC_E);
        case QK_KB_4:
            return LGUI(KC_R);
        case QK_KB_5:
            return LALT(KC_F4);
        case QK_KB_6:
            return LGUI(KC_D);
        case QK_KB_7:
            return LGUI(LALT(KC_R));
        default:
            return keycode;
    }
}

static void tap_dance_open_circuitcurios_repo(void) {
    tap_code16(LGUI(KC_R));
    wait_ms(250);
    SEND_STRING("C:\\GitHub\\CircuitCurios-brand-system");
    tap_code(KC_ENT);
}

static void tap_dance_double_press(uint16_t keycode) {
    if (keycode == QK_KB_8) {
        tap_dance_open_circuitcurios_repo();
        return;
    }

    keycode = tap_dance_resolve_keycode(keycode);
    if (keycode != KC_NO) {
        register_code16(keycode);
    }
}

static void tap_dance_double_release(uint16_t keycode) {
    if (keycode == QK_KB_8) {
        return;
    }

    keycode = tap_dance_resolve_keycode(keycode);
    if (keycode != KC_NO) {
        unregister_code16(keycode);
    }
}

static void tap_dance_single_press(uint16_t keycode) {
    if (keycode == QK_KB_8) {
        tap_dance_open_circuitcurios_repo();
        return;
    }

    keycode = tap_dance_resolve_keycode(keycode);
    if (keycode != KC_NO) {
        register_code16(keycode);
    }
}

static void tap_dance_single_release(uint16_t keycode) {
    if (keycode == QK_KB_8) {
        return;
    }

    keycode = tap_dance_resolve_keycode(keycode);
    if (keycode != KC_NO) {
        unregister_code16(keycode);
    }
}

static void tap_dance_state_clear(void) {
    tap_dance_state.stage = TD_POSITION_IDLE;
    tap_dance_state.row = 0;
    tap_dance_state.col = 0;
    tap_dance_state.pressed = false;
    tap_dance_state.timer = 0;
    tap_dance_state.single_keycode = KC_NO;
    tap_dance_state.double_keycode = KC_NO;
}

static void tap_dance_finish_single(bool keep_held) {
    if (keep_held) {
        tap_dance_single_press(tap_dance_state.single_keycode);
        tap_dance_state.stage = TD_POSITION_SINGLE_HELD;
    } else {
        tap_dance_single_press(tap_dance_state.single_keycode);
        tap_dance_single_release(tap_dance_state.single_keycode);
        tap_dance_state_clear();
    }
}

static void tap_dance_config_defaults(void) {
    tap_dance_config.magic = TAP_DANCE_CONFIG_MAGIC;

    for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
        for (uint8_t col = 0; col < MATRIX_COLS; col++) {
            tap_dance_config.double_keycode[row][col] = KC_NO;
        }
    }

    // Requested base-layer defaults.
    tap_dance_config.double_keycode[1][0] = QK_KB_0; // Screenshot
    tap_dance_config.double_keycode[2][0] = QK_KB_1; // Clipboard
    tap_dance_config.double_keycode[3][3] = QK_KB_8; // CircuitCurios repo
}

static uint16_t tap_dance_legacy_action_to_keycode(uint8_t action, const legacy_tap_dance_editor_storage_t *legacy) {
    switch (action) {
        case LEGACY_TD_ACTION_NONE:
            return KC_NO;
        case LEGACY_TD_ACTION_SCREENSHOT:
            return QK_KB_0;
        case LEGACY_TD_ACTION_CLIPBOARD:
            return QK_KB_1;
        case LEGACY_TD_ACTION_TASK_MANAGER:
            return QK_KB_2;
        case LEGACY_TD_ACTION_EXPLORER:
            return QK_KB_3;
        case LEGACY_TD_ACTION_RUN:
            return QK_KB_4;
        case LEGACY_TD_ACTION_CLOSE_WINDOW:
            return QK_KB_5;
        case LEGACY_TD_ACTION_DESKTOP:
            return QK_KB_6;
        case LEGACY_TD_ACTION_SCREEN_RECORD:
            return QK_KB_7;
        case LEGACY_TD_ACTION_CIRCUITCURIOS_REPO:
            return QK_KB_8;
        case LEGACY_TD_ACTION_CUSTOM_1:
        case LEGACY_TD_ACTION_CUSTOM_2:
        case LEGACY_TD_ACTION_CUSTOM_3:
        case LEGACY_TD_ACTION_CUSTOM_4: {
            uint8_t index = (uint8_t)(action - LEGACY_TD_ACTION_CUSTOM_1);
            return legacy->custom_action[index];
        }
        default:
            return KC_NO;
    }
}

static bool tap_dance_legacy_config_is_valid(const legacy_tap_dance_editor_storage_t *legacy) {
    for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
        for (uint8_t col = 0; col < MATRIX_COLS; col++) {
            if (legacy->action[row][col] >= LEGACY_TD_ACTION_COUNT) {
                return false;
            }
        }
    }
    return true;
}

static void tap_dance_editor_save(void) {
    eeprom_update_block(&tap_dance_config, (void *)(uintptr_t)TAP_DANCE_CONFIG_EEPROM_ADDR, sizeof(tap_dance_config));
}

static void tap_dance_editor_load(void) {
    eeprom_read_block(&tap_dance_config, (const void *)(uintptr_t)TAP_DANCE_CONFIG_EEPROM_ADDR, sizeof(tap_dance_config));

    if (tap_dance_config.magic == TAP_DANCE_CONFIG_MAGIC) {
        return;
    }

    legacy_tap_dance_editor_storage_t legacy;
    eeprom_read_block(&legacy, (const void *)(uintptr_t)TAP_DANCE_OLD_CONFIG_EEPROM_ADDR, sizeof(legacy));

    tap_dance_config_defaults();

    if (tap_dance_legacy_config_is_valid(&legacy)) {
        for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
            for (uint8_t col = 0; col < MATRIX_COLS; col++) {
                tap_dance_config.double_keycode[row][col] = tap_dance_legacy_action_to_keycode(legacy.action[row][col], &legacy);
            }
        }
    }

    tap_dance_editor_save();
}

static void tap_dance_migrate_legacy_matrix_positions(void) {
    // Replace only the old shortcut-TD encodings. User-chosen normal keycodes
    // are left alone from then on.
    uint16_t keycode;

    keycode = dynamic_keymap_get_keycode(TAP_DANCE_BASE_LAYER, 0, 3);
    if (keycode == TD(0)) {
        dynamic_keymap_set_keycode(TAP_DANCE_BASE_LAYER, 0, 3, KC_BSPC);
    }

    keycode = dynamic_keymap_get_keycode(TAP_DANCE_BASE_LAYER, 1, 0);
    if (keycode == QK_KB_8) {
        dynamic_keymap_set_keycode(TAP_DANCE_BASE_LAYER, 1, 0, KC_S);
    }

    keycode = dynamic_keymap_get_keycode(TAP_DANCE_BASE_LAYER, 2, 0);
    if (keycode == QK_KB_9) {
        dynamic_keymap_set_keycode(TAP_DANCE_BASE_LAYER, 2, 0, KC_Z);
    }

    keycode = dynamic_keymap_get_keycode(TAP_DANCE_BASE_LAYER, 2, 3);
    if (keycode == TD(1)) {
        dynamic_keymap_set_keycode(TAP_DANCE_BASE_LAYER, 2, 3, KC_A);
    }

    keycode = dynamic_keymap_get_keycode(TAP_DANCE_BASE_LAYER, 3, 3);
    if (keycode == QK_KB_10 || keycode == TD(2)) {
        dynamic_keymap_set_keycode(TAP_DANCE_BASE_LAYER, 3, 3, KC_C);
    }

    keycode = dynamic_keymap_get_keycode(TAP_DANCE_BASE_LAYER, 4, 3);
    if (keycode == TD(3)) {
        dynamic_keymap_set_keycode(TAP_DANCE_BASE_LAYER, 4, 3, KC_V);
    }
}

static void tap_dance_editor_set_value(uint8_t *data) {
    uint8_t value_id = data[0];

    switch (value_id) {
        case TAP_DANCE_VALUE_SINGLE: {
            uint8_t row = data[1];
            uint8_t col = data[2];
            if (!tap_dance_position_editable(row, col)) {
                break;
            }

            uint16_t keycode = ((uint16_t)data[3] << 8) | data[4];
            dynamic_keymap_set_keycode(TAP_DANCE_BASE_LAYER, row, col, keycode);
            break;
        }

        case TAP_DANCE_VALUE_DOUBLE: {
            uint8_t row = data[1];
            uint8_t col = data[2];
            if (!tap_dance_position_editable(row, col)) {
                break;
            }

            tap_dance_config.double_keycode[row][col] = ((uint16_t)data[3] << 8) | data[4];
            break;
        }
    }
}

static void tap_dance_editor_get_value(uint8_t *data) {
    uint8_t value_id = data[0];

    switch (value_id) {
        case TAP_DANCE_VALUE_SINGLE: {
            uint8_t row = data[1];
            uint8_t col = data[2];
            uint16_t keycode = KC_NO;
            if (tap_dance_position_editable(row, col)) {
                keycode = dynamic_keymap_get_keycode(TAP_DANCE_BASE_LAYER, row, col);
            }

            data[3] = (uint8_t)(keycode >> 8);
            data[4] = (uint8_t)(keycode & 0xFF);
            break;
        }

        case TAP_DANCE_VALUE_DOUBLE: {
            uint8_t row = data[1];
            uint8_t col = data[2];
            uint16_t keycode = KC_NO;
            if (tap_dance_position_editable(row, col)) {
                keycode = tap_dance_config.double_keycode[row][col];
            }

            data[3] = (uint8_t)(keycode >> 8);
            data[4] = (uint8_t)(keycode & 0xFF);
            break;
        }
    }
}

static void tap_dance_editor_command(uint8_t *data, uint8_t length) {
    (void)length;

    uint8_t *command_id = &data[0];
    uint8_t *value_id_and_data = &data[2];

    switch (*command_id) {
        case id_custom_set_value:
            tap_dance_editor_set_value(value_id_and_data);
            break;
        case id_custom_get_value:
            tap_dance_editor_get_value(value_id_and_data);
            break;
        case id_custom_save:
            tap_dance_editor_save();
            break;
        default:
            *command_id = id_unhandled;
            break;
    }
}

void tap_dance_editor_pre_process(uint16_t keycode, keyrecord_t *record) {
    (void)keycode;

    if (!record->event.pressed || tap_dance_state.stage != TD_POSITION_WAIT_SECOND) {
        return;
    }

    if (record->event.key.row == tap_dance_state.row && record->event.key.col == tap_dance_state.col) {
        return;
    }

    tap_dance_finish_single(tap_dance_state.pressed);
}

bool tap_dance_editor_process(uint16_t keycode, keyrecord_t *record) {
    uint8_t row = record->event.key.row;
    uint8_t col = record->event.key.col;

    // Releases belonging to an already intercepted dance must be consumed even
    // if a layer changed while the key was held.
    if (!record->event.pressed && tap_dance_state.stage != TD_POSITION_IDLE &&
        row == tap_dance_state.row && col == tap_dance_state.col) {
        switch (tap_dance_state.stage) {
            case TD_POSITION_WAIT_SECOND:
                tap_dance_state.pressed = false;
                break;

            case TD_POSITION_SINGLE_HELD:
                tap_dance_single_release(tap_dance_state.single_keycode);
                tap_dance_state_clear();
                break;

            case TD_POSITION_DOUBLE_HELD:
                tap_dance_double_release(tap_dance_state.double_keycode);
                tap_dance_state_clear();
                break;

            case TD_POSITION_IDLE:
            default:
                break;
        }
        return true;
    }

    if (!record->event.pressed) {
        return false;
    }

    if (tap_dance_state.stage == TD_POSITION_WAIT_SECOND &&
        row == tap_dance_state.row && col == tap_dance_state.col) {
        if (tap_dance_state.pressed) {
            return true;
        }

        if (timer_elapsed(tap_dance_state.timer) > TAPPING_TERM) {
            tap_dance_finish_single(false);
        } else {
            tap_dance_double_press(tap_dance_state.double_keycode);
            tap_dance_state.stage = TD_POSITION_DOUBLE_HELD;
            tap_dance_state.pressed = true;
            return true;
        }
    }

    if (tap_dance_state.stage != TD_POSITION_IDLE) {
        return false;
    }

    uint8_t layer = get_highest_layer(layer_state | default_layer_state);
    if (layer != TAP_DANCE_BASE_LAYER || !tap_dance_position_editable(row, col)) {
        return false;
    }

    uint16_t double_keycode = tap_dance_config.double_keycode[row][col];
    if (double_keycode == KC_NO) {
        return false;
    }

    tap_dance_state.stage = TD_POSITION_WAIT_SECOND;
    tap_dance_state.row = row;
    tap_dance_state.col = col;
    tap_dance_state.pressed = true;
    tap_dance_state.timer = timer_read();
    tap_dance_state.single_keycode = keycode;
    tap_dance_state.double_keycode = double_keycode;
    return true;
}

void tap_dance_editor_task(void) {
    if (tap_dance_state.stage == TD_POSITION_WAIT_SECOND && timer_elapsed(tap_dance_state.timer) > TAPPING_TERM) {
        tap_dance_finish_single(tap_dance_state.pressed);
    }
}

// Extend VIA with a dedicated coordinate-based tap-dance channel while
// preserving QMK RGB Matrix and the keyboard's layer-lighting channel.
void via_custom_value_command(uint8_t *data, uint8_t length) {
    uint8_t channel_id = data[1];

    if (channel_id == TAP_DANCE_VIA_CHANNEL) {
        tap_dance_editor_command(data, length);
        return;
    }

#if defined(RGB_MATRIX_ENABLE)
    if (channel_id == id_qmk_rgb_matrix_channel) {
        via_qmk_rgb_matrix_command(data, length);
        return;
    }
#endif

    via_custom_value_command_kb(data, length);
}

void keyboard_post_init_kb(void) {
    tap_dance_editor_load();
    keyboard_post_init_user();
    tap_dance_migrate_legacy_matrix_positions();
}
