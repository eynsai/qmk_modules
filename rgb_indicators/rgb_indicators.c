// Copyright 2025 Morgan Newell Sun
// SPDX-License-Identifier: GPL-2.0-or-later

#include "rgb_indicators.h"

ASSERT_COMMUNITY_MODULES_MIN_API_VERSION(1, 1, 0);

// ============================================================================
// STATE
// ============================================================================

typedef enum rgb_indicator_step_t {
    RGB_INDICATOR_STEP_FADE_IN = 0,
    RGB_INDICATOR_STEP_HOLD,
    RGB_INDICATOR_STEP_FADE_OUT,
    RGB_INDICATOR_STEP_STATIC,
    RGB_INDICATOR_STEP_BREATHING,
} rgb_indicator_step_t;

static size_t transition_idx;
static size_t state_idx;
static rgb_indicator_step_t current_step;

static uint32_t lerp_start_time;
static uint32_t lerp_duration;
static float lerp_duration_inv;
static HSV lerp_color_initial;
static HSV lerp_color_final;
static bool breathing_forward;

static uint32_t last_update_time = 0;
static HSV last_set_color;

// ============================================================================
// INTERNAL FUNCTIONS
// ============================================================================

static void hsv_simplify_pair(HSV *a, HSV *b) {
    if (a->v == 0 || a->s == 0) {
        a->h = b->h;
    } else if (b->v == 0 || b->s == 0) {
        b->h = a->h;
    }
    if (a->v == 0) {
        a->s = b->s;
    } else if (b->v == 0) {
        b->s = a->s;
    }
}

static HSV hsv_lerp(HSV initial, HSV final, float ratio) {
    HSV result;
    if (ratio <= 0.0f) return initial;
    if (ratio >= 1.0f) return final;

    int16_t h_diff = (int16_t)final.h - (int16_t)initial.h;
    if (initial.v == 0) {
        h_diff = 0;
    } else if (h_diff > 127) {
        h_diff -= 256;
    } else if (h_diff < -127) {
        h_diff += 256;
    }

    int16_t h_interp = (int16_t)initial.h + (int16_t)(h_diff * ratio);
    if (h_interp < 0) h_interp += 256;
    else if (h_interp > 255) h_interp -= 256;

    result.h = (uint8_t)h_interp;
    result.s = (uint8_t)(initial.s + ((int16_t)final.s - (int16_t)initial.s) * ratio);
    result.v = (uint8_t)(initial.v + ((int16_t)final.v - (int16_t)initial.v) * ratio);
    return result;
}

static void hsv_set(HSV color) {
    last_set_color = color;
    rgblight_sethsv_noeeprom(
        color.h,
        color.s,
        color.v <= RGBLIGHT_LIMIT_VAL ? color.v : RGBLIGHT_LIMIT_VAL
    );
}

// ============================================================================
// MODULE API
// ============================================================================

void keyboard_post_init_rgb_indicators(void) {
    state_idx = 0; // Default to first state
    transition_idx = 0; // No transition in progress

    const rgb_indicator_state_t *initial_state = &rgb_indicator_states[state_idx];

    if (initial_state->breathing) {
        current_step = RGB_INDICATOR_STEP_BREATHING;
        lerp_start_time = timer_read32();
        lerp_duration = initial_state->period_ms;
        lerp_duration_inv = (lerp_duration > 0) ? (1.0f / (float)lerp_duration) : 1.0f;
        lerp_color_initial = initial_state->color_a;
        lerp_color_final = initial_state->color_b;
        hsv_simplify_pair(&lerp_color_initial, &lerp_color_final);
        breathing_forward = true;
        hsv_set(lerp_color_initial);
    } else {
        current_step = RGB_INDICATOR_STEP_STATIC;
        hsv_set(initial_state->color_a);
    }
}

void housekeeping_task_rgb_indicators(void) {
    uint32_t now = timer_read32();
    if (timer_elapsed32(last_update_time) < RGB_INDICATORS_UPDATE_INTERVAL) {
        return;
    }
    last_update_time = now;

    if (current_step == RGB_INDICATOR_STEP_STATIC) {
        // No processing needed
        return;
    }

    uint32_t elapsed = now - lerp_start_time;

    // Handle step transitions
    if (elapsed >= lerp_duration) {
        switch (current_step) {
            case RGB_INDICATOR_STEP_FADE_IN:
                current_step = RGB_INDICATOR_STEP_HOLD;
                lerp_start_time = now;
                lerp_duration = rgb_indicator_transitions[transition_idx].hold_ms;
                lerp_duration_inv = (lerp_duration > 0) ? (1.0f / (float)lerp_duration) : 1.0f;
                lerp_color_initial = rgb_indicator_transitions[transition_idx].accent_color;
                lerp_color_final = rgb_indicator_transitions[transition_idx].accent_color;
                hsv_simplify_pair(&lerp_color_initial, &lerp_color_final);
                break;

            case RGB_INDICATOR_STEP_HOLD:
                current_step = RGB_INDICATOR_STEP_FADE_OUT;
                lerp_start_time = now;
                lerp_duration = rgb_indicator_transitions[transition_idx].fade_out_ms;
                lerp_duration_inv = (lerp_duration > 0) ? (1.0f / (float)lerp_duration) : 1.0f;
                lerp_color_initial = rgb_indicator_transitions[transition_idx].accent_color;
                {
                    const rgb_indicator_state_t *target = &rgb_indicator_states[state_idx];
                    if (target->breathing) {
                        int16_t d_a = abs((int16_t)target->color_a.h - (int16_t)lerp_color_initial.h);
                        int16_t d_b = abs((int16_t)target->color_b.h - (int16_t)lerp_color_initial.h);
                        if (d_a > 127) d_a = 256 - d_a;
                        if (d_b > 127) d_b = 256 - d_b;
                        if (d_a < d_b) {
                            lerp_color_final = target->color_a;
                            breathing_forward = true;
                        } else {
                            lerp_color_final = target->color_b;
                            breathing_forward = false;
                        }
                    } else {
                        lerp_color_final = target->color_a;
                    }
                }
                hsv_simplify_pair(&lerp_color_initial, &lerp_color_final);
                break;

            case RGB_INDICATOR_STEP_FADE_OUT: {
                const rgb_indicator_state_t *target = &rgb_indicator_states[state_idx];
                if (target->breathing) {
                    current_step = RGB_INDICATOR_STEP_BREATHING;
                    lerp_start_time = now;
                    lerp_duration = target->period_ms;
                    lerp_duration_inv = (lerp_duration > 0) ? (1.0f / (float)lerp_duration) : 1.0f;
                    if (breathing_forward) {
                        lerp_color_initial = target->color_a;
                        lerp_color_final = target->color_b;
                    } else {
                        lerp_color_initial = target->color_b;
                        lerp_color_final = target->color_a;
                    }
                    hsv_simplify_pair(&lerp_color_initial, &lerp_color_final);
                } else {
                    current_step = RGB_INDICATOR_STEP_STATIC;
                    hsv_set(lerp_color_final);
                    return;
                }
                break;
            }

            case RGB_INDICATOR_STEP_BREATHING:
                lerp_start_time = now;
                {
                    HSV tmp = lerp_color_initial;
                    lerp_color_initial = lerp_color_final;
                    lerp_color_final = tmp;
                }
                hsv_simplify_pair(&lerp_color_initial, &lerp_color_final);
                break;

            default:
                return;
        }

        // Reset elapsed time after transitioning
        elapsed = 0;
    }

    // Interpolation step
    float ratio = elapsed * lerp_duration_inv;
    HSV current = hsv_lerp(lerp_color_initial, lerp_color_final, ratio);
    hsv_set(current);
}

// ============================================================================
// USER API
// ============================================================================

void rgb_indicators_start_transition(uint8_t t_idx, uint8_t s_idx) {
    transition_idx = t_idx;
    state_idx = s_idx;

    current_step = RGB_INDICATOR_STEP_FADE_IN;
    lerp_start_time = timer_read32();
    lerp_duration = rgb_indicator_transitions[transition_idx].fade_in_ms;
    lerp_duration_inv = (lerp_duration > 0) ? (1.0f / (float)lerp_duration) : 1.0f;
    lerp_color_initial = last_set_color;
    lerp_color_final = rgb_indicator_transitions[transition_idx].accent_color;
    hsv_simplify_pair(&lerp_color_initial, &lerp_color_final);
}
