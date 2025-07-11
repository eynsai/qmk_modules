#include QMK_KEYBOARD_H
#include "quantum.h"
#include "os_detection.h"

// ============================================================================
// STATE
// ============================================================================

typedef enum special_arrow_mods_state_t {
    SPECIAL_ARROW_MODS_STATE_NORMAL = 0,
    SPECIAL_ARROW_MODS_STATE_VSCODE_ADD_CURSOR,
    SPECIAL_ARROW_MODS_STATE_CONDITIONAL_CTRL
} special_arrow_mods_state_t;

static special_arrow_mods_state_t state = SPECIAL_ARROW_MODS_STATE_NORMAL;
uint16_t current_arrow_keycode = KC_NO;

// ============================================================================
// MODULE API
// ============================================================================

#define VSCODE_MODS \
    ((detected_host_os() == OS_LINUX) \
        ? (MOD_BIT(KC_LSFT) | MOD_BIT(KC_LALT)) \
        : (MOD_BIT(KC_LCTL) | MOD_BIT(KC_LALT)))

bool process_record_special_arrow_mods(uint16_t keycode, keyrecord_t *record) {

    // keep track of arrow keys
    uint16_t previous_arrow_keycode = current_arrow_keycode;
    if (keycode == KC_RIGHT || keycode == KC_LEFT || keycode == KC_DOWN || keycode == KC_UP) {
        if (record->event.pressed) {
            current_arrow_keycode = keycode;
        } else {
            if (current_arrow_keycode == keycode) {
                current_arrow_keycode = KC_NO;
            }
        }
    }

    // state machine
    switch (state) {
        
        case SPECIAL_ARROW_MODS_STATE_NORMAL:
            if (keycode == KC_VSCODE_ADD_CURSOR) {
                if (record->event.pressed) {
                    state = SPECIAL_ARROW_MODS_STATE_VSCODE_ADD_CURSOR;
                    if (current_arrow_keycode == KC_UP || current_arrow_keycode == KC_DOWN) {
                        register_mods(VSCODE_MODS);
                    }
                }
                return false;
            } else if (keycode == KC_CONDITIONAL_CTRL) {
                if (record->event.pressed) {
                    state = SPECIAL_ARROW_MODS_STATE_CONDITIONAL_CTRL;
                    if (current_arrow_keycode == KC_LEFT || current_arrow_keycode == KC_RIGHT) {
                        register_mods(MOD_BIT(KC_LCTL));
                    }
                }
                return false;
            } else {
                return true;
            }

        case SPECIAL_ARROW_MODS_STATE_VSCODE_ADD_CURSOR:
            if (keycode == KC_VSCODE_ADD_CURSOR) {;
                if (!record->event.pressed) {
                    if (current_arrow_keycode == KC_UP || current_arrow_keycode == KC_DOWN) {
                        unregister_mods(VSCODE_MODS);
                    }
                    state = SPECIAL_ARROW_MODS_STATE_NORMAL;
                }
                return false;
            } else if (keycode == KC_CONDITIONAL_CTRL) {
                return false;
            } else {
                bool was_up_down = (previous_arrow_keycode == KC_UP || previous_arrow_keycode == KC_DOWN);
                bool is_up_down  = (current_arrow_keycode == KC_UP || current_arrow_keycode == KC_DOWN);
                if (was_up_down && !is_up_down) {
                    unregister_mods(VSCODE_MODS);
                } else if (!was_up_down && is_up_down) {
                    register_mods(VSCODE_MODS);
                }
                return true;
            }

        case SPECIAL_ARROW_MODS_STATE_CONDITIONAL_CTRL:
            if (keycode == KC_VSCODE_ADD_CURSOR) {
                return false;
            } else if (keycode == KC_CONDITIONAL_CTRL) {
                if (!record->event.pressed) {
                    if (current_arrow_keycode == KC_LEFT || current_arrow_keycode == KC_RIGHT) {
                        unregister_mods(MOD_BIT(KC_LCTL));
                    }
                    state = SPECIAL_ARROW_MODS_STATE_NORMAL;
                }
                return false;
            } else {
                bool was_left_right = (previous_arrow_keycode == KC_LEFT || previous_arrow_keycode == KC_RIGHT);
                bool is_left_right  = (current_arrow_keycode == KC_LEFT || current_arrow_keycode == KC_RIGHT);
                if (was_left_right && !is_left_right) {
                    unregister_mods(MOD_BIT(KC_LCTL));
                } else if (!was_left_right && is_left_right) {
                    register_mods(MOD_BIT(KC_LCTL));
                }
                return true;
            }
        default:
            return true;
    }
}
