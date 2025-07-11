// Copyright 2025 Morgan Newell Sun
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "report.h"

enum mouse_passthrough_states {
    MOUSE_PASSTHROUGH_DISCONNECTED = 0,
    MOUSE_PASSTHROUGH_HUB_CONNECTED,
    MOUSE_PASSTHROUGH_REMOTE_CONNECTED,
};

enum report_structure_common {
    REPORT_OFFSET_COMMAND_ID = 0,
    REPORT_OFFSET_DEVICE_ID,
};

enum report_structure_registration {
    REPORT_OFFSET_REGISTRATION = 2,
};

enum report_structure_status {
    REPORT_OFFSET_DEVICE_ID_SELF = 2,
    REPORT_OFFSET_DEVICE_ID_OTHERS
};

enum report_structure_mouse_passthrough {
    REPORT_OFFSET_HANDSHAKE = 2,
    REPORT_OFFSET_DATA_BUTTONS,
    REPORT_OFFSET_DATA_X_MSB,
    REPORT_OFFSET_DATA_X_LSB,
    REPORT_OFFSET_DATA_Y_MSB,
    REPORT_OFFSET_DATA_Y_LSB,
    REPORT_OFFSET_DATA_V_MSB,
    REPORT_OFFSET_DATA_V_LSB,
    REPORT_OFFSET_DATA_H_MSB,
    REPORT_OFFSET_DATA_H_LSB,
    REPORT_OFFSET_CONTROL_BLOCK_BUTTONS,
    REPORT_OFFSET_CONTROL_BLOCK_POINTER,
    REPORT_OFFSET_CONTROL_BLOCK_WHEEL,
    REPORT_OFFSET_CONTROL_SEND_BUTTONS,
    REPORT_OFFSET_CONTROL_SEND_POINTER,
    REPORT_OFFSET_CONTROL_SEND_WHEEL,
    REPORT_OFFSET_RESET,
};

#ifdef MOUSE_PASSTHROUGH_SENDER
bool is_mouse_passthrough_connected(void);
#endif

#ifdef MOUSE_PASSTHROUGH_RECEIVER
bool is_mouse_passthrough_connected(void);
void mouse_passthrough_set_buttons_state(bool, bool);
void mouse_passthrough_set_pointer_state(bool, bool);
void mouse_passthrough_set_wheel_state(bool, bool);
void mouse_passthrough_send_reset_command(void);
#endif
