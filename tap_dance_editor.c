#include QMK_KEYBOARD_H

#include "eeprom.h"
#include "timer.h"
#include "via.h"

#define TAP_DANCE_VIA_CHANNEL 6
#define TAP_DANCE_VALUE_SINGLE 1
#define TAP_DANCE_VALUE_DOUBLE 2
#define EDITABLE_TAP_DANCE_COUNT 4
#define TAP_DANCE_CONFIG_MAGIC 0x54445031UL
#define TAP_DANCE_CONFIG_RESERVED_SIZE 32U
#define TAP_DANCE_CONFIG_EEPROM_ADDR (DYNAMIC_KEYMAP_EEPROM_MAX_ADDR + 1U)
#define TAP_DANCE_ALIAS_FIRST QK_KB_8
#define TAP_DANCE_ALIAS_LAST QK_KB_11

#ifndef TAPPING_TERM
#    define TAPPING_TERM 200
#endif

// These are the first four tap-dance entries in keymaps/via/keymap.c.
// The same four actions are also exposed as VIA custom keycodes TD1..TD4,
// so a slot can be placed on any editable matrix position without changing
// the actual tap-dance action indexes.
static const uint8_t editable_tap_dance_action_index[EDITABLE_TAP_DANCE_COUNT] = {0, 1, 2, 3};

static const tap_dance_pair_t editable_tap_dance_defaults[EDITABLE_TAP_DANCE_COUNT] = {
    {KC_BSPC, KC_ESC},
    {KC_A, LGUI(KC_D)},
    {KC_C, LGUI(KC_E)},
    {KC_V, LGUI(KC_H)},
};

typedef struct {
    uint32_t magic;
    tap_dance_pair_t pairs[EDITABLE_TAP_DANCE_COUNT];
} tap_dance_editor_storage_t;

typedef enum {
    TD_ALIAS_IDLE = 0,
    TD_ALIAS_WAIT_SECOND,
    TD_ALIAS_SINGLE_HELD,
    TD_ALIAS_DOUBLE_HELD,
} tap_dance_alias_stage_t;

typedef struct {
    tap_dance_alias_stage_t stage;
    uint8_t slot;
    bool pressed;
    uint16_t timer;
} tap_dance_alias_state_t;

STATIC_ASSERT(sizeof(tap_dance_editor_storage_t) <= TAP_DANCE_CONFIG_RESERVED_SIZE, "Tap dance editor EEPROM reservation is too small");

extern tap_dance_action_t tap_dance_actions[];
void via_custom_value_command_kb(uint8_t *data, uint8_t length);

static tap_dance_alias_state_t alias_state = {0};

static tap_dance_pair_t *editable_tap_dance_pair(uint8_t slot) {
    if (slot >= EDITABLE_TAP_DANCE_COUNT) {
        return NULL;
    }

    return (tap_dance_pair_t *)tap_dance_actions[editable_tap_dance_action_index[slot]].user_data;
}

static bool tap_dance_alias_slot(uint16_t keycode, uint8_t *slot) {
    if (keycode < TAP_DANCE_ALIAS_FIRST || keycode > TAP_DANCE_ALIAS_LAST) {
        return false;
    }

    *slot = (uint8_t)(keycode - TAP_DANCE_ALIAS_FIRST);
    return *slot < EDITABLE_TAP_DANCE_COUNT;
}

static void tap_dance_alias_clear(void) {
    alias_state.stage = TD_ALIAS_IDLE;
    alias_state.slot = 0;
    alias_state.pressed = false;
    alias_state.timer = 0;
}

static void tap_dance_alias_finish_single(bool keep_held) {
    tap_dance_pair_t *pair = editable_tap_dance_pair(alias_state.slot);
    if (pair == NULL) {
        tap_dance_alias_clear();
        return;
    }

    if (keep_held) {
        register_code16(pair->kc1);
        alias_state.stage = TD_ALIAS_SINGLE_HELD;
    } else {
        tap_code16(pair->kc1);
        tap_dance_alias_clear();
    }
}

static void tap_dance_editor_apply_pairs(const tap_dance_pair_t *pairs) {
    for (uint8_t i = 0; i < EDITABLE_TAP_DANCE_COUNT; i++) {
        tap_dance_pair_t *target = editable_tap_dance_pair(i);
        if (target != NULL) {
            *target = pairs[i];
        }
    }
}

static void tap_dance_editor_save(void) {
    tap_dance_editor_storage_t storage = {
        .magic = TAP_DANCE_CONFIG_MAGIC,
    };

    for (uint8_t i = 0; i < EDITABLE_TAP_DANCE_COUNT; i++) {
        tap_dance_pair_t *pair = editable_tap_dance_pair(i);
        storage.pairs[i] = pair != NULL ? *pair : editable_tap_dance_defaults[i];
    }

    eeprom_update_block(&storage, (void *)(uintptr_t)TAP_DANCE_CONFIG_EEPROM_ADDR, sizeof(storage));
}

static void tap_dance_editor_load(void) {
    tap_dance_editor_storage_t storage;
    eeprom_read_block(&storage, (const void *)(uintptr_t)TAP_DANCE_CONFIG_EEPROM_ADDR, sizeof(storage));

    if (storage.magic != TAP_DANCE_CONFIG_MAGIC) {
        tap_dance_editor_apply_pairs(editable_tap_dance_defaults);
        tap_dance_editor_save();
        return;
    }

    tap_dance_editor_apply_pairs(storage.pairs);
}

static void tap_dance_editor_set_value(uint8_t *data) {
    uint8_t value_id = data[0];
    uint8_t slot = data[1];
    tap_dance_pair_t *pair = editable_tap_dance_pair(slot);

    if (pair == NULL) {
        return;
    }

    uint16_t keycode = ((uint16_t)data[2] << 8) | data[3];

    switch (value_id) {
        case TAP_DANCE_VALUE_SINGLE:
            pair->kc1 = keycode;
            break;
        case TAP_DANCE_VALUE_DOUBLE:
            pair->kc2 = keycode;
            break;
    }
}

static void tap_dance_editor_get_value(uint8_t *data) {
    uint8_t value_id = data[0];
    uint8_t slot = data[1];
    tap_dance_pair_t *pair = editable_tap_dance_pair(slot);

    if (pair == NULL) {
        data[2] = 0;
        data[3] = 0;
        return;
    }

    uint16_t keycode = KC_NO;

    switch (value_id) {
        case TAP_DANCE_VALUE_SINGLE:
            keycode = pair->kc1;
            break;
        case TAP_DANCE_VALUE_DOUBLE:
            keycode = pair->kc2;
            break;
    }

    data[2] = (uint8_t)(keycode >> 8);
    data[3] = (uint8_t)(keycode & 0xFF);
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

// Resolve an unfinished first tap before another key is processed. A second
// press of the same TD slot is left alone so it can become the double tap.
void tap_dance_editor_pre_process(uint16_t keycode, keyrecord_t *record) {
    if (!record->event.pressed || alias_state.stage != TD_ALIAS_WAIT_SECOND) {
        return;
    }

    uint8_t incoming_slot = 0;
    if (tap_dance_alias_slot(keycode, &incoming_slot) && incoming_slot == alias_state.slot) {
        return;
    }

    tap_dance_alias_finish_single(alias_state.pressed);
}

// Handle the VIA-visible TD1..TD4 aliases (QK_KB_8..QK_KB_11). This mirrors
// ACTION_TAP_DANCE_DOUBLE closely enough to preserve single-vs-double behavior,
// while making the slot itself assignable through VIA's normal Custom palette.
bool tap_dance_editor_process(uint16_t keycode, keyrecord_t *record) {
    uint8_t slot = 0;
    if (!tap_dance_alias_slot(keycode, &slot)) {
        return false;
    }

    if (record->event.pressed) {
        if (alias_state.stage == TD_ALIAS_WAIT_SECOND && alias_state.slot == slot) {
            if (alias_state.pressed) {
                return true;
            }

            if (timer_elapsed(alias_state.timer) > TAPPING_TERM) {
                tap_dance_alias_finish_single(false);
            } else {
                tap_dance_pair_t *pair = editable_tap_dance_pair(slot);
                if (pair != NULL) {
                    register_code16(pair->kc2);
                    alias_state.stage = TD_ALIAS_DOUBLE_HELD;
                    alias_state.pressed = true;
                } else {
                    tap_dance_alias_clear();
                }
                return true;
            }
        }

        if (alias_state.stage == TD_ALIAS_IDLE) {
            alias_state.stage = TD_ALIAS_WAIT_SECOND;
            alias_state.slot = slot;
            alias_state.pressed = true;
            alias_state.timer = timer_read();
        }
        return true;
    }

    if (alias_state.slot != slot) {
        return true;
    }

    switch (alias_state.stage) {
        case TD_ALIAS_WAIT_SECOND:
            alias_state.pressed = false;
            break;

        case TD_ALIAS_SINGLE_HELD: {
            tap_dance_pair_t *pair = editable_tap_dance_pair(slot);
            if (pair != NULL) {
                unregister_code16(pair->kc1);
            }
            tap_dance_alias_clear();
            break;
        }

        case TD_ALIAS_DOUBLE_HELD: {
            tap_dance_pair_t *pair = editable_tap_dance_pair(slot);
            if (pair != NULL) {
                unregister_code16(pair->kc2);
            }
            tap_dance_alias_clear();
            break;
        }

        case TD_ALIAS_IDLE:
        default:
            break;
    }

    return true;
}

void tap_dance_editor_task(void) {
    if (alias_state.stage == TD_ALIAS_WAIT_SECOND && timer_elapsed(alias_state.timer) > TAPPING_TERM) {
        tap_dance_alias_finish_single(alias_state.pressed);
    }
}

// Extend VIA with a dedicated Tap Dances channel while preserving QMK's
// built-in RGB Matrix channel and the existing keyboard custom channel used by
// the layer-lighting editor in keymaps/via/keymap.c.
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
}
