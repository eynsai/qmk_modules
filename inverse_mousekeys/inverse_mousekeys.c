// Copyright 2025 Morgan Newell Sun
// SPDX-License-Identifier: GPL-2.0-or-later

#include QMK_KEYBOARD_H
#include "quantum.h"
#include "report.h"

ASSERT_COMMUNITY_MODULES_MIN_API_VERSION(1, 1, 0);

// ============================================================================
// STATE
// ============================================================================

static uint8_t prev_buttons = 0;

// ============================================================================
// MODULE API
// ============================================================================

report_mouse_t pointing_device_task_inverse_mousekeys(report_mouse_t mouse_report) {
    uint8_t changed = mouse_report.buttons ^ prev_buttons;
    uint8_t mask = 1;
    uint16_t now = timer_read();

    for (uint8_t i = 0; i < 8; i++, mask <<= 1) {
        if (changed & mask) {
            keyrecord_t record = {0};
            record.event.key.row = 0;
            record.event.key.col = 0;
            record.event.pressed = (mouse_report.buttons & mask) != 0;
            record.event.time = now;

            uint16_t kc = KC_INV_MOUSEKEY_BUTTON_1 + i;
            if (!(process_record_modules(kc, &record) && process_record_kb(kc, &record))) {
                mouse_report.buttons ^= mask;
            }
        }
    }
    prev_buttons = mouse_report.buttons;

    if (mouse_report.v) {
        keyrecord_t record = {0};
        record.event.key.row = 0;
        record.event.key.col = 0;
        record.event.pressed = 1;
        record.event.time = now;

        uint16_t kc = (mouse_report.v > 0) ? KC_INV_MOUSEKEY_WHEEL_UP : KC_INV_MOUSEKEY_WHEEL_DOWN;
        if (!(process_record_modules(kc, &record) && process_record_kb(kc, &record))) {
            mouse_report.v = 0;
        }
    }

    if (mouse_report.h) {
        keyrecord_t record = {0};
        record.event.key.row = 0;
        record.event.key.col = 0;
        record.event.pressed = 1;
        record.event.time = now;

        uint16_t kc = (mouse_report.h > 0) ? KC_INV_MOUSEKEY_WHEEL_RIGHT : KC_INV_MOUSEKEY_WHEEL_LEFT;
        if (!(process_record_modules(kc, &record) && process_record_kb(kc, &record))) {
            mouse_report.h = 0;
        }
    }

    return mouse_report;
}
