#include QMK_KEYBOARD_H
#include "quantum.h"
#include "os_detection.h"

ASSERT_COMMUNITY_MODULES_MIN_API_VERSION(1, 1, 0);

// ============================================================================
// STATE
// ============================================================================

typedef enum special_keys_state_t {
    SPECIAL_KEYS_STATE_NORMAL = 0,
    SPECIAL_KEYS_STATE_VSCODE_ADD_CURSOR,
    SPECIAL_KEYS_STATE_CONDITIONAL_CTRL,
    SPECIAL_KEYS_STATE_LAZY_ALT_WAITING,
    SPECIAL_KEYS_STATE_LAZY_ALT_ACTIVE,
} special_keys_state_t;

static special_keys_state_t state = SPECIAL_KEYS_STATE_NORMAL;
uint16_t current_arrow_keycode = KC_NO;

// ============================================================================
// MODULE API
// ============================================================================

#define VSCODE_MODS \
    ((detected_host_os() == OS_LINUX) \
        ? (MOD_BIT(KC_LSFT) | MOD_BIT(KC_LALT)) \
        : (MOD_BIT(KC_LCTL) | MOD_BIT(KC_LALT)))

bool process_record_special_keys(uint16_t keycode, keyrecord_t *record) {

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
        
        case SPECIAL_KEYS_STATE_NORMAL:

            if (keycode == KC_DELETE_LINE) {
                if (record->event.pressed) {
                    // tap_code(KC_MINUS);  // at one point I thought this was necessary - why?
                    tap_code(KC_END);
                    tap_code(KC_END);
                    tap_code16(S(KC_HOME));
                    tap_code16(S(KC_HOME));
                    tap_code16(S(KC_LEFT));
                    tap_code(KC_BSPC);
                }
                return false;
            } else if (keycode == KC_VSCODE_ADD_CURSOR) {
                if (record->event.pressed) {
                    state = SPECIAL_KEYS_STATE_VSCODE_ADD_CURSOR;
                    if (current_arrow_keycode == KC_UP || current_arrow_keycode == KC_DOWN) {
                        register_mods(VSCODE_MODS);
                    }
                }
                return false;
            } else if (keycode == KC_CONDITIONAL_CTRL) {
                if (record->event.pressed) {
                    state = SPECIAL_KEYS_STATE_CONDITIONAL_CTRL;
                    if (current_arrow_keycode == KC_LEFT || current_arrow_keycode == KC_RIGHT) {
                        register_mods(MOD_BIT(KC_LCTL));
                    }
                }
                return false;
            } else if (keycode == KC_LAZY_ALT) {
                if (record->event.pressed) {
                    if (current_arrow_keycode == KC_NO) {
                        state = SPECIAL_KEYS_STATE_LAZY_ALT_WAITING;
                    } else {
                        state = SPECIAL_KEYS_STATE_LAZY_ALT_ACTIVE;
                        register_mods(MOD_BIT(KC_LALT));
                    }
                }
                return false;
            } else {
                return true;
            }

        case SPECIAL_KEYS_STATE_VSCODE_ADD_CURSOR:
            if (keycode == KC_VSCODE_ADD_CURSOR) {;
                if (!record->event.pressed) {
                    if (current_arrow_keycode == KC_UP || current_arrow_keycode == KC_DOWN) {
                        unregister_mods(VSCODE_MODS);
                    }
                    state = SPECIAL_KEYS_STATE_NORMAL;
                }
                return false;
            } else if (keycode == KC_CONDITIONAL_CTRL) {
                return false;
            } else if (keycode == KC_LAZY_ALT) {
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

        case SPECIAL_KEYS_STATE_CONDITIONAL_CTRL:
            if (keycode == KC_VSCODE_ADD_CURSOR) {
                return false;
            } else if (keycode == KC_CONDITIONAL_CTRL) {
                if (!record->event.pressed) {
                    if (current_arrow_keycode == KC_LEFT || current_arrow_keycode == KC_RIGHT) {
                        unregister_mods(MOD_BIT(KC_LCTL));
                    }
                    state = SPECIAL_KEYS_STATE_NORMAL;
                }
                return false;
            } else if (keycode == KC_LAZY_ALT) {
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

        case SPECIAL_KEYS_STATE_LAZY_ALT_WAITING:
            if (keycode == KC_VSCODE_ADD_CURSOR) {
                return false;
            } else if (keycode == KC_CONDITIONAL_CTRL) {
                return false;
            } else if (keycode == KC_LAZY_ALT) {
                if (!record->event.pressed) {
                    state = SPECIAL_KEYS_STATE_NORMAL;
                }
                return false;
            } else {
                if (current_arrow_keycode != KC_NO) {
                    state = SPECIAL_KEYS_STATE_LAZY_ALT_ACTIVE;
                    register_mods(MOD_BIT(KC_LALT));
                }
                return true;
            }

        case SPECIAL_KEYS_STATE_LAZY_ALT_ACTIVE:
            if (keycode == KC_VSCODE_ADD_CURSOR) {
                return false;
            } else if (keycode == KC_CONDITIONAL_CTRL) {
                return false;
            } else if (keycode == KC_LAZY_ALT) {
                if (!record->event.pressed) {
                    state = SPECIAL_KEYS_STATE_NORMAL;
                    unregister_mods(MOD_BIT(KC_LALT));
                }
                return false;
            } else {
                return true;
            }

        // shouldn't ever get here
        default:
            return false;
    }
}
