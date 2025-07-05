// Copyright 2025 Morgan Newell Sun
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "report.h"

#ifndef WHEEL_EXTENDED_REPORT
#    error "Must enable WHEEL_EXTENDED_REPORT for this module to work."
#endif  // WHEEL_EXTENDED_REPORT

#ifndef POINTING_DEVICE_HIRES_SCROLL_ENABLE
#    error "Must enable POINTING_DEVICE_HIRES_SCROLL_ENABLE for this module to work."
#endif  // POINTING_DEVICE_HIRES_SCROLL_ENABLE

typedef struct hires_dragscroll_config_t {
    bool vertical_wheel_only;
    bool ctrl_when_vertical;
    bool shift_when_vertical;
    bool alt_when_vertical;
    bool ctrl_when_horizontal;
    bool shift_when_horizontal;
    bool alt_when_horizontal;    
} hires_dragscroll_config_t;

report_mouse_t pre_hires_dragscroll_accumulate_task_kb(report_mouse_t mouse_report);
report_mouse_t pre_hires_dragscroll_accumulate_task_user(report_mouse_t mouse_report);
report_mouse_t pre_hires_dragscroll_scroll_task_kb(report_mouse_t mouse_report);
report_mouse_t pre_hires_dragscroll_scroll_task_user(report_mouse_t mouse_report);
void hires_dragscroll_on(void);
void hires_dragscroll_on_without_axis_snapping(void);
void hires_dragscroll_on_with_config(hires_dragscroll_config_t);
void hires_dragscroll_off(void);
bool is_hires_dragscroll_on(void);
