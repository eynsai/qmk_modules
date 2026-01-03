// Copyright 2025 Morgan Newell Sun
// SPDX-License-Identifier: GPL-2.0-or-later

#include "eynsai_statemachine.h"
#include QMK_KEYBOARD_H
#include "quantum.h"
#include "hires_dragscroll.h"
#include "inverse_mousekeys.h"
#include "mouse_buffer.h"
#include "mouse_passthrough.h"
#include "rgb_indicators.h"

#ifdef CONSOLE_ENABLE
#    include "print.h"
#endif

ASSERT_COMMUNITY_MODULES_MIN_API_VERSION(1, 1, 0);

// ============================================================================
// RGB INDICATOR DEFINITIONS
// ============================================================================

typedef enum indicator_state_t {
    INDICATOR_STATE_OFF = 0,
    INDICATOR_STATE_BASE,
    INDICATOR_STATE_ONESHOT,
} indicator_state_t;

typedef enum indicator_transition_t {
    INDICATOR_TRANSITION_TO_CTRL = 0,
    INDICATOR_TRANSITION_TO_ALT,
    INDICATOR_TRANSITION_TO_GUI,
    INDICATOR_TRANSITION_TO_QWER,
    INDICATOR_TRANSITION_TO_GAME,
    INDICATOR_TRANSITION_FROM_CTRL,
    INDICATOR_TRANSITION_FROM_ALT,
    INDICATOR_TRANSITION_FROM_GUI,
    INDICATOR_TRANSITION_FROM_QWER,
    INDICATOR_TRANSITION_FROM_GAME,
    INDICATOR_TRANSITION_FROM_MULTIPLE,
    INDICATOR_TRANSITION_FLASH_NEUTRAL,
    INDICATOR_TRANSITION_FLASH_CTRL,
    INDICATOR_TRANSITION_FLASH_ALT,
    INDICATOR_TRANSITION_FLASH_GUI,
    INDICATOR_TRANSITION_FLASH_BITWIG,
} indicator_transition_t;

const rgb_indicator_state_t rgb_indicator_states[] = {
    [INDICATOR_STATE_OFF] = { .breathing = false, .color_a = {0, 0, 0} },
    [INDICATOR_STATE_BASE] = { .breathing = false, .color_a = COLOR_NEUTRAL },
    [INDICATOR_STATE_ONESHOT] = { .breathing = true, .color_a = COLOR_ONESHOT_A, .color_b = COLOR_ONESHOT_B, .period_ms = 4000 },
};
const rgb_indicator_transition_t rgb_indicator_transitions[] = {
    [INDICATOR_TRANSITION_TO_CTRL] = { .accent_color = COLOR_CTRL, TIMING_TO },
    [INDICATOR_TRANSITION_TO_ALT] = { .accent_color = COLOR_ALT, TIMING_TO },
    [INDICATOR_TRANSITION_TO_GUI] = { .accent_color = COLOR_GUI, TIMING_TO },
    [INDICATOR_TRANSITION_TO_QWER] = { .accent_color = COLOR_QWER, TIMING_TO },
    [INDICATOR_TRANSITION_TO_GAME] = { .accent_color = COLOR_GAME, TIMING_TO },
    [INDICATOR_TRANSITION_FROM_CTRL] = { .accent_color = COLOR_CTRL, TIMING_FROM },
    [INDICATOR_TRANSITION_FROM_ALT] = { .accent_color = COLOR_ALT, TIMING_FROM },
    [INDICATOR_TRANSITION_FROM_GUI] = { .accent_color = COLOR_GUI, TIMING_FROM },
    [INDICATOR_TRANSITION_FROM_QWER] = { .accent_color = COLOR_QWER, TIMING_FROM },
    [INDICATOR_TRANSITION_FROM_GAME] = { .accent_color = COLOR_GAME, TIMING_FROM },
    [INDICATOR_TRANSITION_FROM_MULTIPLE] = { .accent_color = COLOR_ONESHOT_MIDPOINT, TIMING_FROM },
    [INDICATOR_TRANSITION_FLASH_NEUTRAL] = { .accent_color = COLOR_NEUTRAL, TIMING_FLASH },
    [INDICATOR_TRANSITION_FLASH_CTRL] = { .accent_color = COLOR_CTRL, TIMING_FLASH },
    [INDICATOR_TRANSITION_FLASH_ALT] = { .accent_color = COLOR_ALT, TIMING_FLASH },
    [INDICATOR_TRANSITION_FLASH_GUI] = { .accent_color = COLOR_GUI, TIMING_FLASH },
    [INDICATOR_TRANSITION_FLASH_BITWIG] = { .accent_color = COLOR_BITWIG, TIMING_FLASH },
};

// ============================================================================
// EVENT WATCHERS 
// ============================================================================

static deferred_token timer_token = INVALID_DEFERRED_TOKEN;
static bool timer_callback_is_active = false;
static uint32_t timer_callback_return_value = 0;

uint32_t timer_callback(uint32_t trigger_time, void *cb_arg) {
    keyrecord_t dummy_record = {0};
    dummy_record.event.key.row = 0;
    dummy_record.event.key.col = 0;
    dummy_record.event.pressed = false;
    dummy_record.event.time = timer_read32();
    timer_token = INVALID_DEFERRED_TOKEN;
    timer_callback_is_active = true;
    timer_callback_return_value = 0;
    process_record_eynsai_statemachine(KC_TIMEOUT_EVENT, &dummy_record);
    timer_callback_is_active = false;
    return timer_callback_return_value;
}

void timer_on(uint32_t delay_ms) {
    if (timer_callback_is_active) {
        timer_callback_return_value = delay_ms;
    } else if (timer_token == INVALID_DEFERRED_TOKEN) {
        timer_token = defer_exec(delay_ms, timer_callback, NULL);
    } else {
        extend_deferred_exec(timer_token, delay_ms);
    }
}

void timer_off(void) {
    if (timer_callback_is_active) {
        timer_callback_return_value = 0;
    } else if (timer_token != INVALID_DEFERRED_TOKEN) {
        cancel_deferred_exec(timer_token);
        timer_token = INVALID_DEFERRED_TOKEN;
    }
}

static bool dragscroll_detection_active = false;
static uint32_t dragscroll_detection_accumulator = 0;

void dragscroll_detection_on(void) {
    dragscroll_detection_active = true;
    dragscroll_detection_accumulator = 0;
}

void dragscroll_detection_off(void) {
    dragscroll_detection_active = false;
    dragscroll_detection_accumulator = 0;
}

report_mouse_t post_hires_dragscroll_scroll_task_user(report_mouse_t mouse_report) {
    if (dragscroll_detection_active) {
        dragscroll_detection_accumulator += (uint32_t)(abs(mouse_report.h) + abs(mouse_report.v));
        if (dragscroll_detection_accumulator >= DRAGSCROLL_DETECTION_DEADZONE) {
            keyrecord_t dummy_record = {0};
            dummy_record.event.key.row = 0;
            dummy_record.event.key.col = 0;
            dummy_record.event.pressed = false;
            dummy_record.event.time = timer_read32();
            process_record_eynsai_statemachine(KC_DRAGSCROLL_EVENT, &dummy_record);
            dragscroll_detection_off();
        }
    }
    return mouse_report;
}

// ============================================================================
// STATE
// ============================================================================

typedef enum fsm_node_t {

    // neutral
    FSM_NEUTRAL = 0,
    
    // superctrl
    FSM_CTRL_AMBIGUOUS,
    FSM_CTRL_HELD,
    FSM_CTRL_MODIFIER,
    FSM_CTRL_MOUSE,
    FSM_UTIL_ONESHOT_WAITING,
    FSM_UTIL_ONESHOT_ACTIVE,
    FSM_DRAGSCROLL,

    // superalt
    FSM_ALT_AMBIGUOUS,
    FSM_ALT_HELD,
    FSM_MOVE_MOMENTARY,
    FSM_ALT_MOUSE,
    // FSM_ALT_ONESHOT_WAITING,
    // FSM_ALT_ONESHOT_ACTIVE,

    // supergui
    FSM_GUI_AMBIGUOUS,
    FSM_GUI_HELD,
    FSM_FUNC_MOMENTARY,
    // FSM_GUI_ONESHOT_WAITING,
    // FSM_GUI_ONESHOT_ACTIVE,

    // composite oneshot
    FSM_COMPOSITE_ONESHOT_WAITING,
    FSM_COMPOSITE_ONESHOT_ACTIVE,

    // base layer switching
    FSM_BASE_AMBIGUOUS,

} fsm_node_t;

static fsm_node_t state = FSM_NEUTRAL;

// auxillary state variables
static keymap_layer_t base_layer = LAYER_WORK;
static uint8_t composite_oneshot_mod_bits = 0;
static bool bitwig_mode_is_on = false;

// ============================================================================
// HELPERS
// ============================================================================

#define IS_MODIFIABLE_KEY(kc) ((((kc) & 0xE0FF) >= KC_A && ((kc) & 0xE0FF) <= KC_F24))  // || ((((kc) & 0xE0FF) >= KC_LEFT_CTRL && ((kc) & 0xE0FF) <= KC_RIGHT_GUI)))
#define IS_SHIFT_KEY(kc) ((kc) == KC_LSFT || (kc) == KC_RSFT)
#define IS_SUPER_KEY(kc) ((kc) >= KC_SUPERCTRL && (kc) <= KC_SUPERGUI)
#define IS_STATEMACHINE_KEY(kc) ((kc) >= KC_SUPERCTRL && (kc) <= KC_DRAGSCROLL_EVENT)
#define IS_SYNTHETIC_KEY(kc) (IS_INVERSE_MOUSEKEY((kc)) || IS_STATEMACHINE_KEY((kc)))

static hires_dragscroll_config_t bitwig_scroll_config = {
    .vertical_wheel_only = true,
    .shift_when_vertical = true,
    .shift_when_horizontal = true,
    .alt_when_horizontal = true,
    .invert_horizontal = true,
};

static void transition_to_neutral(void) {
    clear_keyboard();
    hires_dragscroll_off();
    mouse_passthrough_set_buttons_state(false, false);
    mouse_passthrough_set_wheel_state(false, false);
    mouse_passthrough_set_pointer_state(false, false);
    dragscroll_detection_off();
    timer_off();
    layer_off(LAYER_UTIL);
    layer_off(LAYER_MOVE);
    layer_off(LAYER_FUNC);
    state = FSM_NEUTRAL;
    composite_oneshot_mod_bits = 0;
}

// ============================================================================
// MODULE API
// ============================================================================

bool process_record_eynsai_statemachine(uint16_t keycode, keyrecord_t *record) {

    // special case for momentary keycodes, since they're mostly stateless and we don't want them interacting with the statemachine
    if (IS_QK_MOMENTARY(keycode)) return true;

    // special case for shift, since there are virtually no scenarios where we want to block or remap it
    if (IS_SHIFT_KEY(keycode)) return true;

    switch (state) {

        case FSM_NEUTRAL:
            if (record->event.pressed && keycode == KC_SUPERCTRL) {
                state = FSM_CTRL_AMBIGUOUS;
                timer_on(SUPERCTRL_TAPPING_TERM);
                mouse_passthrough_set_buttons_state(true, true);
                mouse_passthrough_set_wheel_state(true, true);
                clear_keyboard_but_mods();
                return false;
            }
            if (record->event.pressed && keycode == KC_SUPERALT) {
                state = FSM_ALT_AMBIGUOUS;
                timer_on(SUPERALT_TAPPING_TERM);
                mouse_passthrough_set_buttons_state(true, true);
                mouse_passthrough_set_wheel_state(true, true);
                clear_keyboard_but_mods();
                layer_on(LAYER_MOVE);
                return false;
            }
            if (record->event.pressed && keycode == KC_SUPERGUI) {
                state = FSM_GUI_AMBIGUOUS;
                timer_on(SUPERGUI_TAPPING_TERM);
                clear_keyboard_but_mods();
                layer_on(LAYER_FUNC);
                return false;
            }
            if (record->event.pressed && keycode == KC_BASE) {
                if (base_layer == LAYER_WORK) {
                    state = FSM_BASE_AMBIGUOUS;
                    timer_on(BASE_TAPPING_TERM);
                } else if (base_layer == LAYER_QWER) {
                    state = FSM_NEUTRAL;
                    base_layer = LAYER_WORK;
                    layer_off(LAYER_QWER);
                    layer_off(LAYER_GAME);
                    rgb_indicators_start_transition(INDICATOR_TRANSITION_FROM_QWER, INDICATOR_STATE_OFF);
                    transition_to_neutral();
                } else if (base_layer == LAYER_GAME) {
                    state = FSM_NEUTRAL;
                    base_layer = LAYER_WORK;
                    layer_off(LAYER_QWER);
                    layer_off(LAYER_GAME);
                    rgb_indicators_start_transition(INDICATOR_TRANSITION_FROM_GAME, INDICATOR_STATE_OFF);
                    transition_to_neutral();
                }
                return false;
            }
            return true;

        // --------------------------------------------------------------------
        // SUPERCTRL
        // --------------------------------------------------------------------

        case FSM_CTRL_AMBIGUOUS:
            if (record->event.pressed && IS_MODIFIABLE_KEY(keycode)) {
                state = FSM_CTRL_MODIFIER;
                timer_off();
                register_code(KC_LCTL);
                return true;
            }
            if (record->event.pressed && IS_INVERSE_MOUSEKEY_BUTTON(keycode)) {
                state = FSM_CTRL_MOUSE;
                timer_off();
                register_code(KC_LCTL);
                mouse_buffer_on(MOUSE_BUFFER_DURATION);
                return true;
            }
            if (record->event.pressed && IS_INVERSE_MOUSEKEY_WHEEL(keycode)) {
                state = FSM_CTRL_MOUSE;
                register_code(KC_LCTL);
                if (bitwig_mode_is_on) {
                    register_code(KC_LALT);
                }
                timer_off();
                mouse_buffer_on(MOUSE_BUFFER_DURATION);
                return true;
            }
            if (!record->event.pressed && keycode == KC_SUPERCTRL) {
                state = FSM_UTIL_ONESHOT_WAITING;
                timer_off();
                layer_on(LAYER_UTIL);
                mouse_passthrough_set_pointer_state(true, true);
                dragscroll_detection_on();
                if (bitwig_mode_is_on) {
                    hires_dragscroll_on_with_config(bitwig_scroll_config);
                } else {
                    hires_dragscroll_on();
                }
                rgb_indicators_start_transition(INDICATOR_TRANSITION_TO_CTRL, INDICATOR_STATE_ONESHOT);
                return false;
            }
            if (keycode == KC_TIMEOUT_EVENT) {
                state = FSM_CTRL_HELD;
                timer_on(BITWIG_TAPPING_TERM);
                return false;
            }
            if (record->event.pressed && IS_SUPER_KEY(keycode)) {
                transition_to_neutral();
                return process_record_eynsai_statemachine(keycode, record);
            }
            return false;

        case FSM_CTRL_HELD:
            if (record->event.pressed && IS_MODIFIABLE_KEY(keycode)) {
                state = FSM_CTRL_MODIFIER;
                timer_off();
                register_code(KC_LCTL);
                return true;
            }
            if (record->event.pressed && IS_INVERSE_MOUSEKEY_BUTTON(keycode)) {
                state = FSM_CTRL_MOUSE;
                timer_off();
                register_code(KC_LCTL);
                mouse_buffer_on(MOUSE_BUFFER_DURATION);
                return true;
            }
            if (record->event.pressed && IS_INVERSE_MOUSEKEY_WHEEL(keycode)) {
                state = FSM_CTRL_MOUSE;
                register_code(KC_LCTL);
                if (bitwig_mode_is_on) {
                    register_code(KC_LALT);
                }
                timer_off();
                mouse_buffer_on(MOUSE_BUFFER_DURATION);
                return true;
            }
            if (keycode == KC_TIMEOUT_EVENT) {
                if (bitwig_mode_is_on) {
                    bitwig_mode_is_on = false;
                    rgb_indicators_start_transition(INDICATOR_TRANSITION_FLASH_NEUTRAL, (base_layer == LAYER_WORK ? INDICATOR_STATE_OFF : INDICATOR_STATE_BASE));
                } else {
                    bitwig_mode_is_on = true;
                    rgb_indicators_start_transition(INDICATOR_TRANSITION_FLASH_BITWIG, (base_layer == LAYER_WORK ? INDICATOR_STATE_OFF : INDICATOR_STATE_BASE));
                }
                transition_to_neutral();
                return false;
            }
            if (!record->event.pressed && keycode == KC_SUPERCTRL) {
                transition_to_neutral();
                return false;
            }
            if (record->event.pressed && IS_SUPER_KEY(keycode)) {
                transition_to_neutral();
                return process_record_eynsai_statemachine(keycode, record);
            }
            return false;

        case FSM_CTRL_MODIFIER:
            if (IS_MODIFIABLE_KEY(keycode)) {
                return true;
            }
            if (!record->event.pressed && keycode == KC_SUPERCTRL) {
                transition_to_neutral();
                return false;
            }
            if (record->event.pressed && IS_SUPER_KEY(keycode)) {
                transition_to_neutral();
                return process_record_eynsai_statemachine(keycode, record);
            }
            return false;

        case FSM_CTRL_MOUSE:
            if (IS_MODIFIABLE_KEY(keycode)) {
                return true;
            }
            if (IS_INVERSE_MOUSEKEY(keycode)) {
                return true;
            }
            if (!record->event.pressed && keycode == KC_SUPERCTRL) {
                transition_to_neutral();
                return false;
            }
            if (record->event.pressed && IS_SUPER_KEY(keycode)) {
                transition_to_neutral();
                return process_record_eynsai_statemachine(keycode, record);
            }
            return false;

        case FSM_UTIL_ONESHOT_WAITING:
            if (record->event.pressed && !IS_SYNTHETIC_KEY(keycode)) {
                state = FSM_UTIL_ONESHOT_ACTIVE;
                dragscroll_detection_off();
                return true;
            }
            if (keycode == KC_DRAGSCROLL_EVENT) {
                state = FSM_DRAGSCROLL;
                dragscroll_detection_off();
                layer_off(LAYER_UTIL);
                return false;
            }
            if (record->event.pressed && keycode == KC_SUPERCTRL) {
                rgb_indicators_start_transition(INDICATOR_TRANSITION_FROM_CTRL, (base_layer == LAYER_WORK ? INDICATOR_STATE_OFF : INDICATOR_STATE_BASE));
                transition_to_neutral();
                return false;
            }
            if (record->event.pressed && IS_SUPER_KEY(keycode)) {
                state = FSM_COMPOSITE_ONESHOT_WAITING;
                hires_dragscroll_off();
                mouse_passthrough_set_pointer_state(false, false);
                dragscroll_detection_off();
                layer_off(LAYER_UTIL);
                if (keycode == KC_SUPERALT) {
                    composite_oneshot_mod_bits = MOD_BIT(KC_LCTL) | MOD_BIT(KC_LALT);
                    rgb_indicators_start_transition(INDICATOR_TRANSITION_FLASH_ALT, INDICATOR_STATE_ONESHOT);
                } else {
                    composite_oneshot_mod_bits = MOD_BIT(KC_LCTL) | MOD_BIT(KC_LGUI);
                    rgb_indicators_start_transition(INDICATOR_TRANSITION_FLASH_GUI, INDICATOR_STATE_ONESHOT);
                }
                return false;
            }
            return false;

        case FSM_UTIL_ONESHOT_ACTIVE:
            if (!record->event.pressed) {
                rgb_indicators_start_transition(INDICATOR_TRANSITION_FROM_CTRL, (base_layer == LAYER_WORK ? INDICATOR_STATE_OFF : INDICATOR_STATE_BASE));
                transition_to_neutral();
                return false;
            }
            return false;

        case FSM_DRAGSCROLL:
            if (record->event.pressed && keycode == KC_SUPERCTRL) {
                state = FSM_CTRL_HELD;
                hires_dragscroll_off();
                mouse_passthrough_set_pointer_state(false, false);
                rgb_indicators_start_transition(INDICATOR_TRANSITION_FROM_CTRL, (base_layer == LAYER_WORK ? INDICATOR_STATE_OFF : INDICATOR_STATE_BASE));
                return false;
            }
            if (IS_SYNTHETIC_KEY(keycode)) {
                return false;
            }
            if (record->event.pressed && !IS_SYNTHETIC_KEY(keycode)) {
                rgb_indicators_start_transition(INDICATOR_TRANSITION_FROM_CTRL, (base_layer == LAYER_WORK ? INDICATOR_STATE_OFF : INDICATOR_STATE_BASE));
                transition_to_neutral();
                return false;
            }
            return false;

        // --------------------------------------------------------------------
        // SUPERALT
        // --------------------------------------------------------------------

        case FSM_ALT_AMBIGUOUS:
            if ((record->event.pressed && !IS_SYNTHETIC_KEY(keycode)) || IS_INVERSE_MOUSEKEY_WHEEL(keycode)) {
                state = FSM_MOVE_MOMENTARY;
                timer_off();
                if (IS_INVERSE_MOUSEKEY_WHEEL(keycode)) {
                    tap_code(keycode == KC_INV_MOUSEKEY_WHEEL_UP ? KC_UP : keycode == KC_INV_MOUSEKEY_WHEEL_DOWN ? KC_DOWN : keycode == KC_INV_MOUSEKEY_WHEEL_LEFT ? KC_LEFT : KC_RIGHT);
                    return false;
                } else {
                    return true;
                }
            }
            if (record->event.pressed && IS_INVERSE_MOUSEKEY_BUTTON(keycode)) {
                state = FSM_ALT_MOUSE;
                timer_off();
                layer_off(LAYER_MOVE);
                register_code(KC_LALT);
                mouse_buffer_on(MOUSE_BUFFER_DURATION);
                return true;
            }
            if (!record->event.pressed && keycode == KC_SUPERALT) {
                state = FSM_COMPOSITE_ONESHOT_WAITING;
                timer_off();
                layer_off(LAYER_MOVE);
                composite_oneshot_mod_bits = MOD_BIT(KC_LALT);
                rgb_indicators_start_transition(INDICATOR_TRANSITION_TO_ALT, INDICATOR_STATE_ONESHOT);
                return false;
            }
            if (keycode == KC_TIMEOUT_EVENT) {
                state = FSM_ALT_HELD;
                return false;
            }
            if (record->event.pressed && IS_SUPER_KEY(keycode)) {
                transition_to_neutral();
                return process_record_eynsai_statemachine(keycode, record);
            }
            return false;

        case FSM_ALT_HELD:
            if ((record->event.pressed && !IS_SYNTHETIC_KEY(keycode)) || IS_INVERSE_MOUSEKEY_WHEEL(keycode)) {
                state = FSM_MOVE_MOMENTARY;
                if (IS_INVERSE_MOUSEKEY_WHEEL(keycode)) {
                    tap_code(keycode == KC_INV_MOUSEKEY_WHEEL_UP ? KC_UP : keycode == KC_INV_MOUSEKEY_WHEEL_DOWN ? KC_DOWN : keycode == KC_INV_MOUSEKEY_WHEEL_LEFT ? KC_LEFT : KC_RIGHT);
                    return false;
                } else {
                    return true;
                }
            }
            if (record->event.pressed && IS_INVERSE_MOUSEKEY_BUTTON(keycode)) {
                state = FSM_ALT_MOUSE;
                layer_off(LAYER_MOVE);
                register_code(KC_LALT);
                mouse_buffer_on(MOUSE_BUFFER_DURATION);
                return true;
            }
            if (!record->event.pressed && keycode == KC_SUPERALT) {
                transition_to_neutral();
                return false;
            }
            if (record->event.pressed && IS_SUPER_KEY(keycode)) {
                transition_to_neutral();
                return process_record_eynsai_statemachine(keycode, record);
            }
            return false;

        case FSM_MOVE_MOMENTARY:
            if (!IS_SYNTHETIC_KEY(keycode)) {
                return true;
            }
            if (IS_INVERSE_MOUSEKEY_WHEEL(keycode)) {
                    tap_code(keycode == KC_INV_MOUSEKEY_WHEEL_UP ? KC_UP : keycode == KC_INV_MOUSEKEY_WHEEL_DOWN ? KC_DOWN : keycode == KC_INV_MOUSEKEY_WHEEL_LEFT ? KC_LEFT : KC_RIGHT);
                return false;
            }
            if (!record->event.pressed && keycode == KC_SUPERALT) {
                transition_to_neutral();
                return false;
            }
            if (record->event.pressed && IS_SUPER_KEY(keycode)) {
                transition_to_neutral();
                return process_record_eynsai_statemachine(keycode, record);
            }
            return false;

        case FSM_ALT_MOUSE:
            if (IS_MODIFIABLE_KEY(keycode)) {
                return true;
            }
            if (IS_INVERSE_MOUSEKEY(keycode)) {
                return true;
            }
            if (!record->event.pressed && keycode == KC_SUPERALT) {
                transition_to_neutral();
                return false;
            }
            if (record->event.pressed && IS_SUPER_KEY(keycode)) {
                transition_to_neutral();
                return process_record_eynsai_statemachine(keycode, record);
            }
            return false;

        // --------------------------------------------------------------------
        // SUPERGUI
        // --------------------------------------------------------------------

        case FSM_GUI_AMBIGUOUS:
            if (record->event.pressed && !IS_SYNTHETIC_KEY(keycode)) {
                state = FSM_FUNC_MOMENTARY;
                timer_off();
                return true;
            }
            if (!record->event.pressed && keycode == KC_SUPERGUI) {
                state = FSM_COMPOSITE_ONESHOT_WAITING;
                timer_off();
                layer_off(LAYER_FUNC);
                composite_oneshot_mod_bits = MOD_BIT(KC_LGUI);
                rgb_indicators_start_transition(INDICATOR_TRANSITION_TO_GUI, INDICATOR_STATE_ONESHOT);
                return false;
            }
            if (keycode == KC_TIMEOUT_EVENT) {
                state = FSM_GUI_HELD;
                return false;
            }
            if (record->event.pressed && IS_SUPER_KEY(keycode)) {
                transition_to_neutral();
                return process_record_eynsai_statemachine(keycode, record);
            }
            return false;

        case FSM_GUI_HELD:
            if (record->event.pressed && !IS_SYNTHETIC_KEY(keycode)) {
                state = FSM_FUNC_MOMENTARY;
                return true;
            }
            if (!record->event.pressed && keycode == KC_SUPERGUI) {
                transition_to_neutral();
                return false;
            }
            if (record->event.pressed && IS_SUPER_KEY(keycode)) {
                transition_to_neutral();
                return process_record_eynsai_statemachine(keycode, record);
            }
            return false;

        case FSM_FUNC_MOMENTARY:
            if (!IS_SYNTHETIC_KEY(keycode)) {
                return true;
            }
            if (!record->event.pressed && keycode == KC_SUPERGUI) {
                transition_to_neutral();
                return false;
            }
            if (record->event.pressed && IS_SUPER_KEY(keycode)) {
                transition_to_neutral();
                return process_record_eynsai_statemachine(keycode, record);
            }
            return false;

        // --------------------------------------------------------------------
        // COMPOSITE ONESHOT
        // --------------------------------------------------------------------

        case FSM_COMPOSITE_ONESHOT_WAITING:
            if (record->event.pressed && IS_MODIFIABLE_KEY(keycode)) {
                state = FSM_COMPOSITE_ONESHOT_ACTIVE;
                register_mods(composite_oneshot_mod_bits);
                return true;
            }
            if (record->event.pressed && keycode == KC_SUPERCTRL) {
                if ((composite_oneshot_mod_bits & MOD_BIT(KC_LCTL)) == 0) {
                    composite_oneshot_mod_bits |= MOD_BIT(KC_LCTL);
                    rgb_indicators_start_transition(INDICATOR_TRANSITION_FLASH_CTRL, INDICATOR_STATE_ONESHOT);
                } else if (composite_oneshot_mod_bits == MOD_BIT(KC_LCTL)) {
                    rgb_indicators_start_transition(INDICATOR_TRANSITION_FROM_CTRL, (base_layer == LAYER_WORK ? INDICATOR_STATE_OFF : INDICATOR_STATE_BASE));
                    transition_to_neutral();
                } else {
                    rgb_indicators_start_transition(INDICATOR_TRANSITION_FROM_MULTIPLE, (base_layer == LAYER_WORK ? INDICATOR_STATE_OFF : INDICATOR_STATE_BASE));
                    transition_to_neutral();
                }
                return false;
            }
            if (record->event.pressed && keycode == KC_SUPERALT) {
                if ((composite_oneshot_mod_bits & MOD_BIT(KC_LALT)) == 0) {
                    composite_oneshot_mod_bits |= MOD_BIT(KC_LALT);
                    rgb_indicators_start_transition(INDICATOR_TRANSITION_FLASH_ALT, INDICATOR_STATE_ONESHOT);
                } else if (composite_oneshot_mod_bits == MOD_BIT(KC_LALT)) {
                    rgb_indicators_start_transition(INDICATOR_TRANSITION_FROM_ALT, (base_layer == LAYER_WORK ? INDICATOR_STATE_OFF : INDICATOR_STATE_BASE));
                    transition_to_neutral();
                } else {
                    rgb_indicators_start_transition(INDICATOR_TRANSITION_FROM_MULTIPLE, (base_layer == LAYER_WORK ? INDICATOR_STATE_OFF : INDICATOR_STATE_BASE));
                    transition_to_neutral();
                }
                return false;
            }
            if (record->event.pressed && keycode == KC_SUPERGUI) {
                if ((composite_oneshot_mod_bits & MOD_BIT(KC_LGUI)) == 0) {
                    composite_oneshot_mod_bits |= MOD_BIT(KC_LGUI);
                    rgb_indicators_start_transition(INDICATOR_TRANSITION_FLASH_GUI, INDICATOR_STATE_ONESHOT);
                } else if (composite_oneshot_mod_bits == MOD_BIT(KC_LGUI)) {
                    rgb_indicators_start_transition(INDICATOR_TRANSITION_FROM_GUI, (base_layer == LAYER_WORK ? INDICATOR_STATE_OFF : INDICATOR_STATE_BASE));
                    transition_to_neutral();
                } else {
                    rgb_indicators_start_transition(INDICATOR_TRANSITION_FROM_MULTIPLE, (base_layer == LAYER_WORK ? INDICATOR_STATE_OFF : INDICATOR_STATE_BASE));
                    transition_to_neutral();
                }
                return false;
            }
            return false;

        case FSM_COMPOSITE_ONESHOT_ACTIVE:
            if (!record->event.pressed) {
                if (composite_oneshot_mod_bits == MOD_BIT(KC_LCTL)) {
                    rgb_indicators_start_transition(INDICATOR_TRANSITION_FROM_CTRL, (base_layer == LAYER_WORK ? INDICATOR_STATE_OFF : INDICATOR_STATE_BASE));
                } else if (composite_oneshot_mod_bits == MOD_BIT(KC_LALT)) {
                    rgb_indicators_start_transition(INDICATOR_TRANSITION_FROM_ALT, (base_layer == LAYER_WORK ? INDICATOR_STATE_OFF : INDICATOR_STATE_BASE));
                } else if (composite_oneshot_mod_bits == MOD_BIT(KC_LGUI)) {
                    rgb_indicators_start_transition(INDICATOR_TRANSITION_FROM_GUI, (base_layer == LAYER_WORK ? INDICATOR_STATE_OFF : INDICATOR_STATE_BASE));
                } else {
                    rgb_indicators_start_transition(INDICATOR_TRANSITION_FROM_MULTIPLE, (base_layer == LAYER_WORK ? INDICATOR_STATE_OFF : INDICATOR_STATE_BASE));
                }
                transition_to_neutral();
                return false;
            }
            return false;

        // --------------------------------------------------------------------
        // BASE
        // --------------------------------------------------------------------
        
        case FSM_BASE_AMBIGUOUS:
            if (keycode == KC_TIMEOUT_EVENT) {
                state = FSM_NEUTRAL;
                base_layer = LAYER_GAME;
                layer_off(LAYER_QWER);
                layer_on(LAYER_GAME);
                rgb_indicators_start_transition(INDICATOR_TRANSITION_TO_GAME, INDICATOR_STATE_BASE);
                transition_to_neutral();
                return false;
            }
            if (!record->event.pressed && keycode == KC_BASE) {
                state = FSM_NEUTRAL;
                base_layer = LAYER_QWER;
                layer_on(LAYER_QWER);
                layer_off(LAYER_GAME);
                rgb_indicators_start_transition(INDICATOR_TRANSITION_TO_QWER, INDICATOR_STATE_BASE);
                transition_to_neutral();
                return false;
            }
            return false;

        // shouldn't ever need this
        default:
            return true;
    }       
}
