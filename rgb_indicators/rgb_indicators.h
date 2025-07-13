// Copyright 2025 Morgan Newell Sun
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once
#include "quantum.h"

// Enum example (user-defined)
/*
typedef enum {
    INDICATOR_STATE_OFF,
    INDICATOR_STATE_BASE_LAYER,
    INDICATOR_STATE_GAMING_LAYER
} indicator_state_t;

typedef enum {
    INDICATOR_TRANSITION_RED,
    INDICATOR_TRANSITION_GREEN,
    INDICATOR_TRANSITION_BLUE
} indicator_transition_t;
*/

typedef struct rgb_indicator_state_t {
    bool breathing;           // If true, cycle between colors
    HSV color_a;              // For static, the single color; for breathing, endpoint A
    HSV color_b;              // For breathing, endpoint B
    uint16_t period_ms;       // Breathing period
} rgb_indicator_state_t;

typedef struct rgb_indicator_transition_t {
    HSV accent_color;         // Color to briefly transition through
    uint16_t fade_in_ms;      // Time to lerp to accent
    uint16_t hold_ms;         // Time to hold accent
    uint16_t fade_out_ms;     // Time to lerp to target
} rgb_indicator_transition_t;

extern const rgb_indicator_state_t rgb_indicator_states[];
extern const rgb_indicator_transition_t rgb_indicator_transitions[];

void rgb_indicators_start_transition(uint8_t transition_idx, uint8_t target_state_idx);
