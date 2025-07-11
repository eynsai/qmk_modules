// Copyright 2025 Morgan Newell Sun
// SPDX-License-Identifier: GPL-2.0-or-later

#include "mouse_buffer.h"
#include QMK_KEYBOARD_H
#include "quantum.h"
#include "pointing_device.h"

// ============================================================================
// STATE
// ============================================================================

static bool buffer_active = false;
static uint32_t buffer_start_time = 0;
static uint32_t buffer_duration_ms = 0;

static uint8_t accumulated_buttons = 0;
static mouse_hv_report_t accumulated_v = 0;
static mouse_hv_report_t accumulated_h = 0;

// ============================================================================
// MODULE API
// ============================================================================

report_mouse_t pointing_device_task_mouse_buffer(report_mouse_t mouse_report) {
    if (!buffer_active) {
        return mouse_report;
    }

    if (timer_elapsed32(buffer_start_time) < buffer_duration_ms) {
        // Still buffering: accumulate and suppress
        accumulated_buttons |= mouse_report.buttons;
        accumulated_v += mouse_report.v;
        accumulated_h += mouse_report.h;

        mouse_report.buttons = 0;
        mouse_report.v = 0;
        mouse_report.h = 0;
        return mouse_report;
    }

    // Buffer expired: merge accumulated inputs
    mouse_report.buttons |= accumulated_buttons;
    mouse_report.v += accumulated_v;
    mouse_report.h += accumulated_h;

    accumulated_buttons = 0;
    accumulated_v = 0;
    accumulated_h = 0;
    buffer_active = false;
    return mouse_report;
}

// ============================================================================
// USER API
// ============================================================================

void mouse_buffer_on(uint32_t duration_ms) {
    if (!buffer_active) {
        accumulated_buttons = 0;
        accumulated_v = 0;
        accumulated_h = 0;
        buffer_active = true;
    }
    buffer_start_time = timer_read32();
    buffer_duration_ms = duration_ms;
}
