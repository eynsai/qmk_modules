// Copyright 2026 Morgan Newell Sun
// SPDX-License-Identifier: GPL-2.0-or-later

#include <stdlib.h>
#include <math.h>
#include "quantum.h"
#include "report.h"

ASSERT_COMMUNITY_MODULES_MIN_API_VERSION(1, 1, 0);

// ============================================================================
// STATE
// ============================================================================

typedef enum {
    MOUSE_AXIS_SNAPPING_UNDECIDED = 0,
    MOUSE_AXIS_SNAPPING_HORIZONTAL,
    MOUSE_AXIS_SNAPPING_VERTICAL,
} mouse_axis_snapping_state_t;

bool mouse_axis_snapping_active = false;
float mouse_axis_snapping_deviation = 0;
mouse_axis_snapping_state_t mouse_axis_snapping_state = MOUSE_AXIS_SNAPPING_UNDECIDED;

// ============================================================================
// MODULE API
// ============================================================================

report_mouse_t pointing_device_task_mouse_axis_snapping(report_mouse_t mouse_report) {

    if (!mouse_axis_snapping_active) return mouse_report;

    switch (mouse_axis_snapping_state) {
        case MOUSE_AXIS_SNAPPING_UNDECIDED:
            // we don't know which axis to snap since the user hasn't moved the pointing device
            if (abs(mouse_report.x) > abs(mouse_report.y)) {
                // snap to horizontal axis
                mouse_report.y = 0;
                mouse_axis_snapping_state = MOUSE_AXIS_SNAPPING_HORIZONTAL;
            } else if (abs(mouse_report.x) < abs(mouse_report.y)) {
                // snap to vertical axis
                mouse_report.x = 0;
                mouse_axis_snapping_state = MOUSE_AXIS_SNAPPING_VERTICAL;
            }
            break;
        case MOUSE_AXIS_SNAPPING_HORIZONTAL:
            mouse_axis_snapping_deviation += mouse_report.y;
            if (mouse_axis_snapping_deviation > 0) {
                mouse_axis_snapping_deviation -= abs(mouse_report.x) * MOUSE_AXIS_SNAPPING_RATIO;
                mouse_axis_snapping_deviation = mouse_axis_snapping_deviation < 0 ? 0 : mouse_axis_snapping_deviation;
            } else if (mouse_axis_snapping_deviation < 0) {
                mouse_axis_snapping_deviation += abs(mouse_report.x) * MOUSE_AXIS_SNAPPING_RATIO;
                mouse_axis_snapping_deviation = mouse_axis_snapping_deviation > 0 ? 0 : mouse_axis_snapping_deviation;
            }
            if (fabsf(mouse_axis_snapping_deviation) > MOUSE_AXIS_SNAPPING_THRESHOLD) {
                // switch to the vertical axis
                mouse_report.x = 0;
                mouse_axis_snapping_deviation = 0;
                mouse_axis_snapping_state = MOUSE_AXIS_SNAPPING_VERTICAL;
            } else {
                mouse_report.y = 0;
            }
            break;
        case MOUSE_AXIS_SNAPPING_VERTICAL:
            mouse_axis_snapping_deviation += mouse_report.x;
            if (mouse_axis_snapping_deviation > 0) {
                mouse_axis_snapping_deviation -= abs(mouse_report.y) * MOUSE_AXIS_SNAPPING_RATIO;
                mouse_axis_snapping_deviation = mouse_axis_snapping_deviation < 0 ? 0 : mouse_axis_snapping_deviation;
            } else if (mouse_axis_snapping_deviation < 0) {
                mouse_axis_snapping_deviation += abs(mouse_report.y) * MOUSE_AXIS_SNAPPING_RATIO;
                mouse_axis_snapping_deviation = mouse_axis_snapping_deviation > 0 ? 0 : mouse_axis_snapping_deviation;
            }
            if (fabsf(mouse_axis_snapping_deviation) > MOUSE_AXIS_SNAPPING_THRESHOLD) {
                // switch to the horizontal axis
                mouse_report.y = 0;
                mouse_axis_snapping_deviation = 0;
                mouse_axis_snapping_state = MOUSE_AXIS_SNAPPING_HORIZONTAL;
            } else {
                mouse_report.x = 0;
            }
            break;
    }

    return mouse_report;
}

// ============================================================================
// USER API
// ============================================================================

void mouse_axis_snapping_on(void) {
    if (mouse_axis_snapping_active) return;
    mouse_axis_snapping_active = true;
    mouse_axis_snapping_deviation = 0;
    mouse_axis_snapping_state = MOUSE_AXIS_SNAPPING_UNDECIDED;
}

void mouse_axis_snapping_off(void) {
    mouse_axis_snapping_active = false;
}