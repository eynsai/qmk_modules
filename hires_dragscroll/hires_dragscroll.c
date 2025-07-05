// Copyright 2025 Morgan Newell Sun
// SPDX-License-Identifier: GPL-2.0-or-later

#include <stdlib.h>
#include <math.h>

#include "host_driver.h"
#include "pointing_device.h"
#include "timer.h"
#include "usb_descriptor_common.h"

#include "hires_dragscroll.h"

ASSERT_COMMUNITY_MODULES_MIN_API_VERSION(1, 1, 0);

// ============================================================================
// PARAMETERS
// ============================================================================

// Sensitivity
#ifndef HIRES_DRAGSCROLL_MULTIPLIER_H
#    define HIRES_DRAGSCROLL_MULTIPLIER_H 8.0
#endif  // HIRES_DRAGSCROLL_MULTIPLIER_H
#ifndef HIRES_DRAGSCROLL_MULTIPLIER_V
#    define HIRES_DRAGSCROLL_MULTIPLIER_V 8.0
#endif  // HIRES_DRAGSCROLL_MULTIPLIER_V

// Timings
#ifndef HIRES_DRAGSCROLL_THROTTLE_MS
#    define HIRES_DRAGSCROLL_THROTTLE_MS 16
#endif  // HIRES_DRAGSCROLL_THROTTLE_MS
#ifndef HIRES_DRAGSCROLL_TIMEOUT_MS
#    define HIRES_DRAGSCROLL_TIMEOUT_MS 500
#endif  // HIRES_DRAGSCROLL_TIMEOUT_MS

// Axis snapping thresholds
#ifndef HIRES_DRAGSCROLL_AXIS_SNAPPING_THRESHOLD
#    define HIRES_DRAGSCROLL_AXIS_SNAPPING_THRESHOLD 0.25
#endif  // HIRES_DRAGSCROLL_AXIS_SNAPPING_THRESHOLD
#ifndef HIRES_DRAGSCROLL_AXIS_SNAPPING_RATIO
#    define HIRES_DRAGSCROLL_AXIS_SNAPPING_RATIO 2.0
#endif  // HIRES_DRAGSCROLL_AXIS_SNAPPING_RATIO

// Smoothing amount
#ifndef HIRES_DRAGSCROLL_SMOOTHING_AMOUNT
#    define HIRES_DRAGSCROLL_SMOOTHING_AMOUNT 5
#endif  // HIRES_DRAGSCROLL_SMOOTHING_AMOUNT

// Acceleration curve
#ifndef HIRES_DRAGSCROLL_ACCELERATION_SCALE
#    define HIRES_DRAGSCROLL_ACCELERATION_SCALE 500.0
#endif  // HIRES_DRAGSCROLL_ACCELERATION_SCALE
#ifndef HIRES_DRAGSCROLL_ACCELERATION_BLEND
#    define HIRES_DRAGSCROLL_ACCELERATION_BLEND 0.872116
#endif  // HIRES_DRAGSCROLL_ACCELERATION_BLEND

// ============================================================================
// RING BUFFERS
// ============================================================================

#ifdef HIRES_DRAGSCROLL_SMOOTHING

typedef struct {
    float items[HIRES_DRAGSCROLL_SMOOTHING_AMOUNT];
    float current_sum;
    size_t current_size;
    size_t next_index;
} ring_buffer_t;

static void ring_buffer_reset(ring_buffer_t* rb) {
    rb->current_sum  = 0;
    rb->current_size = 0;
    rb->next_index   = 0;
}

static void ring_buffer_push(ring_buffer_t* rb, float item) {
    if (rb->current_size == HIRES_DRAGSCROLL_SMOOTHING_AMOUNT) {
        rb->current_sum -= rb->items[rb->next_index];
    } else {
        rb->current_size++;
    }
    rb->items[rb->next_index] = item;
    rb->current_sum += item;
    rb->next_index = (rb->next_index + 1) % HIRES_DRAGSCROLL_SMOOTHING_AMOUNT;
}

static float ring_buffer_mean(ring_buffer_t* rb) {
    return rb->current_size > 0 ? rb->current_sum / rb->current_size : 0;
}

#endif  // HIRES_DRAGSCROLL_SMOOTHING

// ============================================================================
// MISC
// ============================================================================

typedef enum {
    AXIS_SNAPPING_UNDECIDED = 0,
    AXIS_SNAPPING_HORIZONTAL,
    AXIS_SNAPPING_VERTICAL,
} axis_snapping_state_t;

// ============================================================================
// STATE
// ============================================================================


bool active = false;
bool axis_snapping = true;
hires_dragscroll_config_t hires_dragscroll_config = {0};
uint32_t last_movement_time;
uint32_t last_scroll_time = 0;

float multiplier_h = HIRES_DRAGSCROLL_MULTIPLIER_H;
float multiplier_v = HIRES_DRAGSCROLL_MULTIPLIER_V;

float accumulator_h;
float accumulator_v;
float rounding_error_h;
float rounding_error_v;

float axis_snapping_deviation;
axis_snapping_state_t axis_snapping_state;

#ifdef HIRES_DRAGSCROLL_SMOOTHING
ring_buffer_t smoothing_buffer_h;
ring_buffer_t smoothing_buffer_v;
#endif  // HIRES_DRAGSCROLL_SMOOTHING

#ifdef HIRES_DRAGSCROLL_ACCELERATION
float acceleration_const_p;
float acceleration_const_q;
float acceleration_const_r;
#endif  // HIRES_DRAGSCROLL_ACCELERATION

// ============================================================================
// CORE FUNCTIONALITY
// ============================================================================

static void hires_dragscroll_reset_task(void) {
    last_movement_time = timer_read32();
    accumulator_h = 0;
    accumulator_v = 0;
    rounding_error_h = 0;
    rounding_error_v = 0;
    axis_snapping_deviation = 0;
    axis_snapping_state = AXIS_SNAPPING_UNDECIDED;
#ifdef HIRES_DRAGSCROLL_SMOOTHING
    ring_buffer_reset(&smoothing_buffer_h);
    ring_buffer_reset(&smoothing_buffer_v);
#endif  // HIRES_DRAGSCROLL_SMOOTHING
}

static report_mouse_t hires_dragscroll_accumulate_task(report_mouse_t mouse_report) {
    if (!active) {
        return mouse_report;
    }
    float delta_h;
    float delta_v;

    // run user code
    mouse_report = pre_hires_dragscroll_accumulate_task_kb(mouse_report);

    // reset drag scroll if the pointing device has been idle for too long
    if (mouse_report.x == 0 && mouse_report.y == 0) {
        if (timer_elapsed32(last_movement_time) > HIRES_DRAGSCROLL_TIMEOUT_MS) {
            hires_dragscroll_reset_task();
        }
        return mouse_report;
    }
    last_movement_time = timer_read32();

    // scale hires scrolling so that hires and normal scrolling have the same speed
    delta_h = ((float)(mouse_report.x)) * pointing_device_get_hires_scroll_resolution();
    delta_v = ((float)(mouse_report.y)) * pointing_device_get_hires_scroll_resolution();

    // update accumulators
    accumulator_h += delta_h;
    accumulator_v += delta_v;

    // zero out the mouse report
    mouse_report.x = 0;
    mouse_report.y = 0;
}

static inline void update_modifiers(void) {
    if (axis_snapping_state == AXIS_SNAPPING_VERTICAL) {
        if (hires_dragscroll_config.ctrl_when_horizontal && !hires_dragscroll_config.ctrl_when_vertical) {
            unregister_code(KC_LCTL);
        }
        if (hires_dragscroll_config.shift_when_horizontal && !hires_dragscroll_config.shift_when_vertical) {
            unregister_code(KC_LSFT);
        }
        if (hires_dragscroll_config.alt_when_horizontal && !hires_dragscroll_config.alt_when_vertical) {
            unregister_code(KC_LALT);
        }
        if (hires_dragscroll_config.ctrl_when_vertical) {
            register_code(KC_LCTL);
        }
        if (hires_dragscroll_config.shift_when_vertical) {
            register_code(KC_LSFT);
        }
        if (hires_dragscroll_config.alt_when_vertical) {
            register_code(KC_LALT);
        }
    } else if (axis_snapping_state == AXIS_SNAPPING_HORIZONTAL) {
        if (hires_dragscroll_config.ctrl_when_vertical && !hires_dragscroll_config.ctrl_when_horizontal) {
            unregister_code(KC_LCTL);
        }
        if (hires_dragscroll_config.shift_when_vertical && !hires_dragscroll_config.shift_when_horizontal) {
            unregister_code(KC_LSFT);
        }
        if (hires_dragscroll_config.alt_when_vertical && !hires_dragscroll_config.alt_when_horizontal) {
            unregister_code(KC_LALT);
        }
        if (hires_dragscroll_config.ctrl_when_horizontal) {
            register_code(KC_LCTL);
        }
        if (hires_dragscroll_config.shift_when_horizontal) {
            register_code(KC_LSFT);
        }
        if (hires_dragscroll_config.alt_when_horizontal) {
            register_code(KC_LALT);
        }
    }
}

static report_mouse_t hires_dragscroll_scroll_task(report_mouse_t mouse_report) {
    if (!active) {
        return mouse_report;
    }
    float h;
    float v;

    // run user code
    mouse_report = pre_hires_dragscroll_scroll_task_kb(mouse_report);

    // apply smoothing
#ifdef HIRES_DRAGSCROLL_SMOOTHING
    ring_buffer_push(&smoothing_buffer_h, accumulator_h);
    ring_buffer_push(&smoothing_buffer_v, accumulator_v);
    h = ring_buffer_mean(&smoothing_buffer_h);
    v = ring_buffer_mean(&smoothing_buffer_v);
#endif  // HIRES_DRAGSCROLL_SMOOTHING

    // zero out the accumulators
    accumulator_h = 0;
    accumulator_v = 0;

    // apply axis snapping (force snapping on if non-default modes are used)
    if (axis_snapping) {
        switch (axis_snapping_state) {
            case AXIS_SNAPPING_UNDECIDED:
                // we don't know which axis to snap since the user hasn't moved the pointing device
                if (fabsf(h) > fabsf(v)) {
                    // snap to horizontal axis
                    v = 0;
                    axis_snapping_state = AXIS_SNAPPING_HORIZONTAL;
                    update_modifiers();
                } else if (fabsf(h) < fabsf(v)) {
                    // snap to vertical axis
                    h = 0;
                    axis_snapping_state = AXIS_SNAPPING_VERTICAL;
                    update_modifiers();
                }
                break;
            case AXIS_SNAPPING_HORIZONTAL:
                axis_snapping_deviation += v;
                if (axis_snapping_deviation > 0) {
                    axis_snapping_deviation -= fabsf(h) * HIRES_DRAGSCROLL_AXIS_SNAPPING_RATIO;
                    axis_snapping_deviation = axis_snapping_deviation < 0 ? 0 : axis_snapping_deviation;
                } else if (axis_snapping_deviation < 0) {
                    axis_snapping_deviation += fabsf(h) * HIRES_DRAGSCROLL_AXIS_SNAPPING_RATIO;
                    axis_snapping_deviation = axis_snapping_deviation > 0 ? 0 : axis_snapping_deviation;
                }
                if (fabsf(axis_snapping_deviation) > HIRES_DRAGSCROLL_AXIS_SNAPPING_THRESHOLD) {
                    // switch to the vertical axis
                    h = 0;
                    rounding_error_h = 0;
                    rounding_error_v = 0;
                    axis_snapping_deviation = 0;
                    axis_snapping_state = AXIS_SNAPPING_VERTICAL;
                    update_modifiers();
#ifdef HIRES_DRAGSCROLL_SMOOTHING
                    ring_buffer_reset(&smoothing_buffer_h);
                    ring_buffer_reset(&smoothing_buffer_v);
#endif  // HIRES_DRAGSCROLL_SMOOTHING
                } else {
                    v = 0;
                }
                break;
            case AXIS_SNAPPING_VERTICAL:
                axis_snapping_deviation += h;
                if (axis_snapping_deviation > 0) {
                    axis_snapping_deviation -= fabsf(v) * HIRES_DRAGSCROLL_AXIS_SNAPPING_RATIO;
                    axis_snapping_deviation = axis_snapping_deviation < 0 ? 0 : axis_snapping_deviation;
                } else if (axis_snapping_deviation < 0) {
                    axis_snapping_deviation += fabsf(v) * HIRES_DRAGSCROLL_AXIS_SNAPPING_RATIO;
                    axis_snapping_deviation = axis_snapping_deviation > 0 ? 0 : axis_snapping_deviation;
                }
                if (fabsf(axis_snapping_deviation) > HIRES_DRAGSCROLL_AXIS_SNAPPING_THRESHOLD) {
                    // switch to the horizontal axis
                    v = 0;
                    rounding_error_h = 0;
                    rounding_error_v = 0;
                    axis_snapping_deviation = 0;
                    axis_snapping_state = AXIS_SNAPPING_HORIZONTAL;
                    update_modifiers();
#ifdef HIRES_DRAGSCROLL_SMOOTHING
                    ring_buffer_reset(&smoothing_buffer_h);
                    ring_buffer_reset(&smoothing_buffer_v);
#endif  // HIRES_DRAGSCROLL_SMOOTHING
                } else {
                    h = 0;
                }
                break;
        }
    }

    // apply acceleration
#ifdef HIRES_DRAGSCROLL_ACCELERATION
    if (!(h == 0 && v == 0)) {
        // v_out = p * square(min(v_in - r, 0)) + q * (v_in - r) + r
        float speed = sqrt(h * h + v * v);
        float speed_offset = speed - acceleration_const_r;
        float scale_factor = acceleration_const_q * speed_offset + acceleration_const_r;
        if (speed_offset < 0) {
            scale_factor += acceleration_const_p * speed_offset * speed_offset;
        }
        scale_factor /= speed;
        h *= scale_factor;
        v *= scale_factor;
    }
#endif  // HIRES_DRAGSCROLL_ACCELERATION

    // apply scaling
    h *= multiplier_h;
    v *= multiplier_v;

    // save rounding errors
    h += rounding_error_h;
    v += rounding_error_v;
    mouse_report.h = (mouse_hv_report_t)h;
    mouse_report.v = (mouse_hv_report_t)v;
    rounding_error_h = h - mouse_report.h;
    rounding_error_v = v - mouse_report.v;

    // apply config
    if (hires_dragscroll_config.vertical_wheel_only) {
        mouse_report.v += mouse_report.h;
        mouse_report.h = 0;
    }

    // return
    return mouse_report;
}

// ============================================================================
// MODULE API
// ============================================================================

#ifdef HIRES_DRAGSCROLL_ACCELERATION
void pointing_device_init_hires_dragscroll(void) {
    float scale = HIRES_DRAGSCROLL_ACCELERATION_SCALE * HIRES_DRAGSCROLL_THROTTLE_MS;  // multiply by HIRES_DRAGSCROLL_THROTTLE_MS so that acceleration behaves the same when HIRES_DRAGSCROLL_THROTTLE_MS is changed
    float blend = HIRES_DRAGSCROLL_ACCELERATION_BLEND;
    scale = scale < 0 ? 0 : scale;
    blend = blend < 0 ? 0 : blend;
    blend = blend > 1 ? 1 : blend;
    acceleration_const_p = blend / scale;
    acceleration_const_q = blend + 1;
    acceleration_const_r = scale;
}
#endif

report_mouse_t pointing_device_task_hires_dragscroll(report_mouse_t mouse_report) {

    // accumulate on every call, but only send a nonzero mouse report periodically
    mouse_report = hires_dragscroll_accumulate_task(mouse_report);
    if (timer_elapsed32(last_scroll_time) < HIRES_DRAGSCROLL_THROTTLE_MS) {
        return mouse_report;
    }
    last_scroll_time = timer_read32();
    mouse_report = hires_dragscroll_scroll_task(mouse_report);
    return mouse_report;
}

// ============================================================================
// USER API
// ============================================================================

__attribute__((weak)) report_mouse_t pre_hires_dragscroll_accumulate_task_kb(report_mouse_t mouse_report) {
    return pre_hires_dragscroll_accumulate_task_user(mouse_report);
}

__attribute__((weak)) report_mouse_t pre_hires_dragscroll_accumulate_task_user(report_mouse_t mouse_report) {
    return mouse_report;
}

__attribute__((weak)) report_mouse_t pre_hires_dragscroll_scroll_task_kb(report_mouse_t mouse_report) {
    return pre_hires_dragscroll_scroll_task_user(mouse_report);
}

__attribute__((weak)) report_mouse_t pre_hires_dragscroll_scroll_task_user(report_mouse_t mouse_report) {
    return mouse_report;
}

void hires_dragscroll_on(void) {
    if (active) {
        return;
    }
    active = true;
    axis_snapping = true;
    hires_dragscroll_config = (hires_dragscroll_config_t){false, false, false, false, false, false, false};
    hires_dragscroll_reset_task();
}

void hires_dragscroll_on_without_axis_snapping(void) {
    if (active) {
        return;
    }
    active = true;
    axis_snapping = false;
    hires_dragscroll_config = (hires_dragscroll_config_t){false, false, false, false, false, false, false};
    hires_dragscroll_reset_task();
}

void hires_dragscroll_on_with_config(hires_dragscroll_config_t config) {
    if (active) {
        return;
    }
    active = true;
    axis_snapping = true;
    hires_dragscroll_config = config;
    hires_dragscroll_reset_task();
}

void hires_dragscroll_off(void) {
    active = false;
}

bool is_hires_dragscroll_on(void) {
    return active;
}