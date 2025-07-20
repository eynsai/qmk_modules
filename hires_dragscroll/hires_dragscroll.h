// Copyright 2025 Morgan Newell Sun
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "report.h"

typedef struct hires_dragscroll_config_t {
    bool vertical_wheel_only;       // convert horizontal motion to VWHEEL instead of HWHEEL
    bool ctrl_when_vertical;        // apply the CTRL modifier when scrolling vertically
    bool shift_when_vertical;       // apply the SHIFT modifier when scrolling vertically
    bool alt_when_vertical;         // apply the ALT modifier when scrolling vertically
    bool invert_vertical;           // invert vertical scrolling direction
    bool ctrl_when_horizontal;      // apply the CTRL modifier when scrolling horizontally
    bool shift_when_horizontal;     // apply the SHIFT modifier when scrolling horizontally
    bool alt_when_horizontal;       // apply the ALT modifier when scrolling horizontally
    bool invert_horizontal;         // invert horizontal scrolling direction
} hires_dragscroll_config_t;

report_mouse_t pre_hires_dragscroll_accumulate_task_kb(report_mouse_t mouse_report);
report_mouse_t pre_hires_dragscroll_accumulate_task_user(report_mouse_t mouse_report);
report_mouse_t pre_hires_dragscroll_scroll_task_kb(report_mouse_t mouse_report);
report_mouse_t pre_hires_dragscroll_scroll_task_user(report_mouse_t mouse_report);
report_mouse_t post_hires_dragscroll_scroll_task_kb(report_mouse_t mouse_report);
report_mouse_t post_hires_dragscroll_scroll_task_user(report_mouse_t mouse_report);
void hires_dragscroll_on(void);
void hires_dragscroll_on_without_axis_snapping(void);
void hires_dragscroll_on_with_config(hires_dragscroll_config_t);
void hires_dragscroll_off(void);
bool is_hires_dragscroll_on(void);
