// Copyright 2026 Morgan Newell Sun
// SPDX-License-Identifier: GPL-2.0-or-later

#include "simple_timer.h"
#include "community_modules.h"
#include QMK_KEYBOARD_H

ASSERT_COMMUNITY_MODULES_MIN_API_VERSION(0, 1, 0);

// ============================================================================
// STATE
// ============================================================================

static deferred_token simple_timer_deferred_token = INVALID_DEFERRED_TOKEN;
static bool currently_inside_callback_wrapper = false;
static uint32_t callback_wrapper_return_value = 0;

// ============================================================================
// USER API
// ============================================================================

uint32_t simple_timer_callback_wrapper(uint32_t trigger_time, void *cb_arg) {

    // normally, this timer callback will turn 0
    // but, if the user turns the timer on in the callback, return a nonzero value to repeat the deferred execution
    callback_wrapper_return_value = 0;
    
    // call the user's callback
    currently_inside_callback_wrapper = true;
    simple_timer_callback();
    currently_inside_callback_wrapper = false;
    
    // if the user's callback ends with the timer off, reset the token
    if (callback_wrapper_return_value == 0) { simple_timer_deferred_token = INVALID_DEFERRED_TOKEN; }

    // return
    return callback_wrapper_return_value;
}

void simple_timer_on(uint32_t delay_ms) {

    // the user's timer callback code is trying to turn the timer on
    if (currently_inside_callback_wrapper) {
        callback_wrapper_return_value = delay_ms;

    // the timer is currently off, so turn it on
    } else if (simple_timer_deferred_token == INVALID_DEFERRED_TOKEN) {
        simple_timer_deferred_token = defer_exec(delay_ms, simple_timer_callback_wrapper, NULL);

    // the timer is already on, so just reset the delay
    } else {
        extend_deferred_exec(simple_timer_deferred_token, delay_ms);
    }
}

void simple_timer_off(void) {

    // the user's timer callback code is trying to turn the timer off
    if (currently_inside_callback_wrapper) {
        callback_wrapper_return_value = 0;

    // the timer is currently on, so turn it off
    } else if (simple_timer_deferred_token != INVALID_DEFERRED_TOKEN) {
        cancel_deferred_exec(simple_timer_deferred_token);
        simple_timer_deferred_token = INVALID_DEFERRED_TOKEN;
    }
}

__attribute__((weak)) void simple_timer_callback(void) {
    return;
}
