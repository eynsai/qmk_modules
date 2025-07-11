// Copyright 2025 Morgan Newell Sun
// SPDX-License-Identifier: GPL-2.0-or-later

#include "eynsai_statemachine.h"
#include QMK_KEYBOARD_H
#include "quantum.h"
#include "hires_dragscroll.h"
#include "inverse_mousekeys.h"
#include "mouse_buffer.h"
#include "mouse_passthrough.h"
// #include "rgb_indicators.h"

#ifdef CONSOLE_ENABLE
#    include "print.h"
#endif

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
    FSM_ALT_ONESHOT_WAITING,
    FSM_ALT_ONESHOT_ACTIVE,

} fsm_node_t;

static fsm_node_t current_state = FSM_NEUTRAL;

// ============================================================================
// EVENT WATCHERS 
// ============================================================================

static deferred_token timer_token = INVALID_DEFERRED_TOKEN;

uint32_t timer_callback(uint32_t trigger_time, void *cb_arg) {
    keyrecord_t dummy_record = {0};
    dummy_record.event.key.row = 0;
    dummy_record.event.key.col = 0;
    dummy_record.event.pressed = false;
    dummy_record.event.time = timer_read32();
    process_record_eynsai_statemachine(KC_TIMEOUT_EVENT, &dummy_record);
    timer_token = INVALID_DEFERRED_TOKEN;
    return 0;
}

void timer_on(uint32_t delay_ms) {
    if (timer_token == INVALID_DEFERRED_TOKEN) {
        timer_token = defer_exec(delay_ms, timer_callback, NULL);
    } else {
        extend_deferred_exec(timer_token, delay_ms);
    }
}

void timer_off(void) {
    if (timer_token != INVALID_DEFERRED_TOKEN) {
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
// HELPERS
// ============================================================================

#define IS_MODIFIABLE_KEY(kc) ((((kc) & 0xE0FF) >= KC_A && ((kc) & 0xE0FF) <= KC_F24) || ((((kc) & 0xE0FF) >= KC_LEFT_CTRL && ((kc) & 0xE0FF) <= KC_RIGHT_GUI)))
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
    // layer_off(LAYER_FUNC);
    current_state = FSM_NEUTRAL;
}

// ============================================================================
// MODULE API
// ============================================================================

bool process_record_eynsai_statemachine(uint16_t keycode, keyrecord_t *record) {

    bool return_value;

    switch (current_state) {

        case FSM_NEUTRAL:
            if (record->event.pressed && keycode == KC_SUPERCTRL) {
                current_state = FSM_CTRL_AMBIGUOUS;
                timer_on(SUPER_TAPPING_TERM);
                mouse_passthrough_set_buttons_state(true, true);
                mouse_passthrough_set_wheel_state(true, true);
                return_value = false;
                break;
            }
            if (record->event.pressed && keycode == KC_SUPERALT) {
                current_state = FSM_ALT_AMBIGUOUS;
                timer_on(SUPER_TAPPING_TERM);
                mouse_passthrough_set_buttons_state(true, true);
                mouse_passthrough_set_wheel_state(true, true);
                layer_on(LAYER_MOVE);
                return_value = false;
                break;
            }
            return_value = true;
            break;

        // --------------------------------------------------------------------
        // SUPERCTRL
        // --------------------------------------------------------------------

        case FSM_CTRL_AMBIGUOUS:
            if (record->event.pressed && IS_MODIFIABLE_KEY(keycode)) {
                current_state = FSM_CTRL_MODIFIER;
                timer_off();
                register_code(KC_LCTL);
                return_value = true;
                break;
            }
            if (record->event.pressed && (IS_INVERSE_MOUSEKEY_BUTTON(keycode) || IS_INVERSE_MOUSEKEY_WHEEL(keycode))) {
                current_state = FSM_CTRL_MOUSE;
                timer_off();
                register_code(KC_LCTL);
                mouse_buffer_on(MOUSE_BUFFER_DURATION);
                return_value = true;
                break;
            }
            if (!record->event.pressed && keycode == KC_SUPERCTRL) {
                current_state = FSM_UTIL_ONESHOT_WAITING;
                timer_off();
                layer_on(LAYER_UTIL);
                mouse_passthrough_set_pointer_state(true, true);
                dragscroll_detection_on();
                // hires_dragscroll_on();
                hires_dragscroll_on_with_config(bitwig_scroll_config);
                return_value = false;
                break;
            }
            if (keycode == KC_TIMEOUT_EVENT) {
                current_state = FSM_CTRL_HELD;
                return_value = false;
                break;
            }
            return_value = false;
            break;

        case FSM_CTRL_HELD:
            if (record->event.pressed && IS_MODIFIABLE_KEY(keycode)) {
                current_state = FSM_CTRL_MODIFIER;
                register_code(KC_LCTL);
                return_value = true;
                break;
            }
            if (record->event.pressed && (IS_INVERSE_MOUSEKEY_BUTTON(keycode) || IS_INVERSE_MOUSEKEY_WHEEL(keycode))) {
                current_state = FSM_CTRL_MOUSE;
                register_code(KC_LCTL);
                mouse_buffer_on(MOUSE_BUFFER_DURATION);
                return_value = true;
                break;
            }
            if (!record->event.pressed && keycode == KC_SUPERCTRL) {
                transition_to_neutral();
                return_value = false;
                break;
            }
            return_value = false;
            break;

        case FSM_CTRL_MODIFIER:
            if (IS_MODIFIABLE_KEY(keycode)) {
                return_value = true;
                break;
            }
            if (!record->event.pressed && keycode == KC_SUPERCTRL) {
                transition_to_neutral();
                return_value = false;
                break;
            }
            return_value = false;
            break;

        case FSM_CTRL_MOUSE:
            if (IS_MODIFIABLE_KEY(keycode)) {
                return_value = true;
                break;
            }
            if (IS_INVERSE_MOUSEKEY(keycode)) {
                return_value = true;
                break;
            }
            if (!record->event.pressed && keycode == KC_SUPERCTRL) {
                transition_to_neutral();
                return_value = false;
                break;
            }
            return_value = false;
            break;

        case FSM_UTIL_ONESHOT_WAITING:
            if (record->event.pressed && !IS_SYNTHETIC_KEY(keycode)) {
                current_state = FSM_UTIL_ONESHOT_ACTIVE;
                dragscroll_detection_off();
                return_value = true;
                break;
            }
            if (keycode == KC_DRAGSCROLL_EVENT) {
                current_state = FSM_DRAGSCROLL;
                dragscroll_detection_off();
                layer_off(LAYER_UTIL);
                return_value = false;
                break;
            }
            if (record->event.pressed && keycode == KC_SUPERCTRL) {
                transition_to_neutral();
                return_value = false;
                break;
            }
            return_value = false;
            break;

        case FSM_DRAGSCROLL:
            if (record->event.pressed && keycode == KC_SUPERCTRL) {
                current_state = FSM_CTRL_HELD;
                hires_dragscroll_off();
                mouse_passthrough_set_pointer_state(false, false);
                return_value = false;
                break;
            }
            if (IS_SYNTHETIC_KEY(keycode)) {
                return_value = false;
                break;
            }
            if (record->event.pressed && !IS_SYNTHETIC_KEY(keycode)) {
                transition_to_neutral();
                return_value = false;
                break;
            }
            return_value = false;
            break;

        case FSM_UTIL_ONESHOT_ACTIVE:
            if (!record->event.pressed) {
                transition_to_neutral();
                return_value = false;
                break;
            }
            return_value = false;
            break;

        // --------------------------------------------------------------------
        // SUPERALT
        // --------------------------------------------------------------------

        case FSM_ALT_AMBIGUOUS:
            if (record->event.pressed && !IS_SYNTHETIC_KEY(keycode)) {
                current_state = FSM_MOVE_MOMENTARY;
                timer_off();
                return_value = true;
                break;
            }
            if (record->event.pressed && IS_INVERSE_MOUSEKEY(keycode)) {
                current_state = FSM_ALT_MOUSE;
                timer_off();
                layer_off(LAYER_MOVE);
                register_code(KC_LALT);
                mouse_buffer_on(MOUSE_BUFFER_DURATION);
                return_value = true;
                break;
            }
            if (!record->event.pressed && keycode == KC_SUPERALT) {
                current_state = FSM_ALT_ONESHOT_WAITING;
                timer_off();
                layer_off(LAYER_MOVE);
                return_value = false;
                break;
            }
            if (keycode == KC_TIMEOUT_EVENT) {
                current_state = FSM_ALT_HELD;
                return_value = false;
                break;
            }
            return_value = false;
            break;

        case FSM_ALT_HELD:
            if (record->event.pressed && !IS_SYNTHETIC_KEY(keycode)) {
                current_state = FSM_MOVE_MOMENTARY;
                return_value = true;
                break;
            }
            if (record->event.pressed && IS_INVERSE_MOUSEKEY(keycode)) {
                current_state = FSM_ALT_MOUSE;
                layer_off(LAYER_MOVE);
                register_code(KC_LALT);
                mouse_buffer_on(MOUSE_BUFFER_DURATION);
                return_value = true;
                break;
            }
            if (!record->event.pressed && keycode == KC_SUPERALT) {
                transition_to_neutral();
                return_value = false;
                break;
            }
            return_value = false;
            break;

        case FSM_MOVE_MOMENTARY:
            if (!IS_SYNTHETIC_KEY(keycode)) {
                return_value = true;
                break;
            }
            if (!record->event.pressed && keycode == KC_SUPERALT) {
                transition_to_neutral();
                return_value = false;
                break;
            }
            return_value = false;
            break;

        case FSM_ALT_MOUSE:
            if (IS_MODIFIABLE_KEY(keycode)) {
                return_value = true;
                break;
            }
            if (IS_INVERSE_MOUSEKEY(keycode)) {
                return_value = true;
                break;
            }
            if (!record->event.pressed && keycode == KC_SUPERALT) {
                transition_to_neutral();
                return_value = false;
                break;
            }
            return_value = false;
            break;

        case FSM_ALT_ONESHOT_WAITING:
            if (record->event.pressed && IS_MODIFIABLE_KEY(keycode)) {
                current_state = FSM_ALT_ONESHOT_ACTIVE;
                register_code(KC_LALT);
                return_value = true;
                break;
            }
            if (record->event.pressed && keycode == KC_SUPERALT) {
                transition_to_neutral();
                return_value = false;
                break;
            }
            return_value = false;
            break;

        case FSM_ALT_ONESHOT_ACTIVE:
            if (!record->event.pressed) {
                transition_to_neutral();
                return_value = false;
                break;
            }
            return_value = false;
            break;

        // shouldn't ever need this
        default:
            return_value = true;
            break;
    }

#ifdef CONSOLE_ENABLE
    uprintf("KL: kc: 0x%04X, pressed: %u, state: %u\n", keycode, record->event.pressed, current_state);
#endif
    return return_value;            
}
