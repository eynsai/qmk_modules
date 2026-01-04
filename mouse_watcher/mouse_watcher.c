// Copyright 2026 Morgan Newell Sun
// SPDX-License-Identifier: GPL-2.0-or-later

#include "mouse_watcher.h"
#include "community_modules.h"
#include QMK_KEYBOARD_H

ASSERT_COMMUNITY_MODULES_MIN_API_VERSION(1, 1, 0);

// ============================================================================
// STATE
// ============================================================================

static bool mouse_watcher_active = false;
static uint16_t mouse_watcher_threshold = 1;
static int32_t mouse_watcher_accumulator_x = 0;
static int32_t mouse_watcher_accumulator_y = 0;

// ============================================================================
// MODULE API
// ============================================================================

report_mouse_t pointing_device_task_mouse_watcher(report_mouse_t mouse_report) {
    if (mouse_watcher_active) {
        mouse_watcher_accumulator_x += (int32_t)(mouse_report.x);
        mouse_watcher_accumulator_y += (int32_t)(mouse_report.y);
        if ((abs(mouse_watcher_accumulator_x) >= mouse_watcher_threshold) || (abs(mouse_watcher_accumulator_y) >= mouse_watcher_threshold)) {
            mouse_watcher_callback();
            mouse_watcher_off();
        }
    }
    return mouse_report;
}

// ============================================================================
// USER API
// ============================================================================

void mouse_watcher_on(uint16_t threshold) {
    mouse_watcher_active = true;
    mouse_watcher_threshold = threshold;
    mouse_watcher_accumulator_x = 0;
    mouse_watcher_accumulator_y = 0;
}

void mouse_watcher_off(void) {
    mouse_watcher_active = false;
}

__attribute__((weak)) void mouse_watcher_callback(void) {
    return;
}