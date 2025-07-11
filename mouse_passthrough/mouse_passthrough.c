// Copyright 2025 Morgan Newell Sun
// SPDX-License-Identifier: GPL-2.0-or-later

#include "mouse_passthrough.h"
#include QMK_KEYBOARD_H
#include "raw_hid.h"

#ifdef MOUSE_PASSTHROUGH_SENDER

// ============================================================================
// STATE
// ============================================================================

static uint8_t state;
static uint8_t device_id_self;
static uint8_t device_id_others[MAX_REGISTERED_DEVICES - 1];
static uint8_t device_id_remote;
static uint32_t last_connection_attempt_time;
static uint32_t last_connection_success_time;
static uint8_t message_queue[QMK_RAW_HID_REPORT_SIZE * MAX_QUEUED_MESSAGES];
static uint8_t message_queue_next_empty_offset = 0;

static uint8_t last_buttons_received = 0;
static uint8_t last_buttons_sent = 0;
static bool block_buttons_on = false;
static bool block_buttons_on_queued = false;
static bool block_pointer_on = false;
static bool block_wheel_on = false;
static bool send_buttons_on = false;
static bool send_buttons_off_queued = false;
static bool send_pointer_on = false;
static bool send_wheel_on = false;

// ============================================================================
// MODULE API
// ============================================================================

void housekeeping_task_mouse_passthrough(void) {

    // we can only send one raw hid message per matrix scan, anything after the first message gets garbled for some reason
    if (message_queue_next_empty_offset != 0) {
        message_queue_next_empty_offset -= QMK_RAW_HID_REPORT_SIZE;
        raw_hid_send(message_queue + message_queue_next_empty_offset, QMK_RAW_HID_REPORT_SIZE);
    }

    if (timer_elapsed32(last_connection_success_time) > HUB_CONNECTION_EXPIRY_INTERVAL) {
        state = MOUSE_PASSTHROUGH_DISCONNECTED;
    }

    if (timer_elapsed32(last_connection_attempt_time) > HUB_CONNECTION_ATTEMPT_INTERVAL) {
        last_connection_attempt_time = timer_read32();
        // send a registration report
        if (message_queue_next_empty_offset < sizeof(message_queue)) {
            memset(message_queue + message_queue_next_empty_offset, 0, QMK_RAW_HID_REPORT_SIZE);
            message_queue[message_queue_next_empty_offset + REPORT_OFFSET_COMMAND_ID] = RAW_HID_HUB_COMMAND_ID;
            message_queue[message_queue_next_empty_offset + REPORT_OFFSET_DEVICE_ID] = 0xFF;
            message_queue[message_queue_next_empty_offset + REPORT_OFFSET_REGISTRATION] = 0x01;
            message_queue_next_empty_offset += QMK_RAW_HID_REPORT_SIZE;
        }
        
        if (state == MOUSE_PASSTHROUGH_HUB_CONNECTED) {
            // handshake step 1/4: mouse broadcasts to all devices
            for (int i = 0; i < MAX_REGISTERED_DEVICES - 1; i++) {
                if (device_id_others[i] != DEVICE_ID_UNASSIGNED) {
                    if (message_queue_next_empty_offset < sizeof(message_queue)) {
                        memset(message_queue + message_queue_next_empty_offset, 0, QMK_RAW_HID_REPORT_SIZE);
                        message_queue[message_queue_next_empty_offset + REPORT_OFFSET_COMMAND_ID] = RAW_HID_HUB_COMMAND_ID;
                        message_queue[message_queue_next_empty_offset + REPORT_OFFSET_DEVICE_ID] = device_id_others[i];
                        message_queue[message_queue_next_empty_offset + REPORT_OFFSET_HANDSHAKE] = 13;
                        message_queue_next_empty_offset += QMK_RAW_HID_REPORT_SIZE;
                    }
                }
            }
        }
    }
}

report_mouse_t pointing_device_task_mouse_passthrough(report_mouse_t mouse) {
    last_buttons_received = mouse.buttons;
    
    if (state != MOUSE_PASSTHROUGH_REMOTE_CONNECTED) {
        return mouse;
    }

    // send data payload
    if (((send_buttons_on && (mouse.buttons != last_buttons_sent)) || (send_pointer_on && ((mouse.x != 0) || (mouse.y != 0))) || (send_wheel_on && ((mouse.v != 0) || (mouse.h != 0)))) && (message_queue_next_empty_offset < sizeof(message_queue))) {
        memset(message_queue + message_queue_next_empty_offset, 0, QMK_RAW_HID_REPORT_SIZE);
        message_queue[message_queue_next_empty_offset + REPORT_OFFSET_COMMAND_ID] = RAW_HID_HUB_COMMAND_ID;
        message_queue[message_queue_next_empty_offset + REPORT_OFFSET_DEVICE_ID] = device_id_remote;
        message_queue[message_queue_next_empty_offset + REPORT_OFFSET_DATA_BUTTONS] = mouse.buttons;
        message_queue[message_queue_next_empty_offset + REPORT_OFFSET_DATA_X_MSB] = (mouse.x >> 8) & 0xFF;
        message_queue[message_queue_next_empty_offset + REPORT_OFFSET_DATA_X_LSB] = mouse.x & 0xFF;
        message_queue[message_queue_next_empty_offset + REPORT_OFFSET_DATA_Y_MSB] = (mouse.y >> 8) & 0xFF;
        message_queue[message_queue_next_empty_offset + REPORT_OFFSET_DATA_Y_LSB] = mouse.y & 0xFF;
        message_queue[message_queue_next_empty_offset + REPORT_OFFSET_DATA_V_MSB] = (mouse.v >> 8) & 0xFF;
        message_queue[message_queue_next_empty_offset + REPORT_OFFSET_DATA_V_LSB] = mouse.v & 0xFF;
        message_queue[message_queue_next_empty_offset + REPORT_OFFSET_DATA_H_MSB] = (mouse.h >> 8) & 0xFF;
        message_queue[message_queue_next_empty_offset + REPORT_OFFSET_DATA_H_LSB] = mouse.h & 0xFF;
        message_queue_next_empty_offset += QMK_RAW_HID_REPORT_SIZE;
        last_buttons_sent = mouse.buttons;
    }

    // block inputs
    if (block_buttons_on) {
        mouse.buttons = 0;
    }
    if (block_pointer_on) {
        mouse.x = 0;
        mouse.y = 0;
    }
    if (block_wheel_on) {
        mouse.v = 0;
        mouse.h = 0;
    }

    // process queued changes in button block/send
    // this must be done last so we can send the button off report
    if (block_buttons_on_queued && mouse.buttons == 0) {
        block_buttons_on = true;
        block_buttons_on_queued = false;
    }
    if (send_buttons_off_queued && mouse.buttons == 0) {
        send_buttons_on = false;
        send_buttons_off_queued = false;
    }

    return mouse;
}

void raw_hid_receive(uint8_t* data, uint8_t length) {

    if (data[REPORT_OFFSET_COMMAND_ID] != RAW_HID_HUB_COMMAND_ID) {
        return;
    }

    last_connection_success_time = timer_read32();
    if (state == MOUSE_PASSTHROUGH_DISCONNECTED) {
        state = MOUSE_PASSTHROUGH_HUB_CONNECTED;
    }

    if (state == MOUSE_PASSTHROUGH_REMOTE_CONNECTED && data[REPORT_OFFSET_DEVICE_ID] == device_id_remote) {
        // unpack control payload
        if (data[REPORT_OFFSET_RESET] > 0) {
            reset_keyboard();
        }
        if (data[REPORT_OFFSET_CONTROL_BLOCK_BUTTONS] > 0) {
            if (!block_buttons_on) {
                if (last_buttons_received == 0) {
                    block_buttons_on = true;
                } else {
                    block_buttons_on_queued = true;
                }
            }
        } else {
            block_buttons_on = false;
            block_buttons_on_queued = false;
        }
        if (data[REPORT_OFFSET_CONTROL_SEND_BUTTONS] == 0) {
            if (send_buttons_on) {
                if (last_buttons_received == 0) {
                    send_buttons_on = false;
                } else {
                    send_buttons_off_queued = true;
                }
            }
        } else {
            send_buttons_on = true;
            send_buttons_off_queued = false;
        }
        block_pointer_on = data[REPORT_OFFSET_CONTROL_BLOCK_POINTER];
        block_wheel_on = data[REPORT_OFFSET_CONTROL_BLOCK_WHEEL];
        send_pointer_on = data[REPORT_OFFSET_CONTROL_SEND_POINTER];
        send_wheel_on = data[REPORT_OFFSET_CONTROL_SEND_WHEEL];

    } else if (data[REPORT_OFFSET_DEVICE_ID] == DEVICE_ID_HUB) {
        if (data[REPORT_OFFSET_DEVICE_ID_SELF] == DEVICE_ID_UNASSIGNED) {
            // hub has shutdown
            state = MOUSE_PASSTHROUGH_DISCONNECTED;
        } else {
            device_id_self = data[REPORT_OFFSET_DEVICE_ID_SELF];
            memcpy(device_id_others, data + REPORT_OFFSET_DEVICE_ID_OTHERS, MAX_REGISTERED_DEVICES - 1);
            if (state == MOUSE_PASSTHROUGH_REMOTE_CONNECTED) {
                // see if we're still connected
                bool found = false;
                for (int i = REPORT_OFFSET_DEVICE_ID_OTHERS; i < QMK_RAW_HID_REPORT_SIZE; i++) {
                    if (data[i] == device_id_remote) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    state = MOUSE_PASSTHROUGH_HUB_CONNECTED;
                }
            }
        }
            
    } else if (state == MOUSE_PASSTHROUGH_HUB_CONNECTED && data[REPORT_OFFSET_HANDSHAKE] == 26) {
        // handshake step 3/4: mouse responds to first keyboard it hears from
        if (message_queue_next_empty_offset < sizeof(message_queue)) {
            state = MOUSE_PASSTHROUGH_REMOTE_CONNECTED;
            device_id_remote = data[REPORT_OFFSET_DEVICE_ID];
            memset(message_queue + message_queue_next_empty_offset, 0, QMK_RAW_HID_REPORT_SIZE);
            message_queue[message_queue_next_empty_offset + REPORT_OFFSET_COMMAND_ID] = RAW_HID_HUB_COMMAND_ID;
            message_queue[message_queue_next_empty_offset + REPORT_OFFSET_DEVICE_ID] = device_id_remote;
            message_queue[message_queue_next_empty_offset + REPORT_OFFSET_HANDSHAKE] = 39;
            message_queue_next_empty_offset += QMK_RAW_HID_REPORT_SIZE;
            block_buttons_on = false;
            block_buttons_on_queued = false;
            block_pointer_on = false;
            block_wheel_on = false;
            send_buttons_on = false;
            send_buttons_off_queued = false;
            send_pointer_on = false;
            send_wheel_on = false;
        }
    }
}

// ============================================================================
// USER API
// ============================================================================

bool is_mouse_passthrough_connected(void) {
    return state == MOUSE_PASSTHROUGH_REMOTE_CONNECTED;
}

#endif  // MOUSE_PASSTHROUGH_SENDER

#ifdef MOUSE_PASSTHROUGH_RECEIVER

// ============================================================================
// STATE
// ============================================================================

static uint8_t state = MOUSE_PASSTHROUGH_DISCONNECTED;
static uint8_t device_id_self;
static uint8_t device_id_others[MAX_REGISTERED_DEVICES - 1];
static uint8_t device_id_remote;
static uint32_t last_connection_attempt_time = 0;
static uint32_t last_connection_success_time = 0;
static uint8_t message_queue[QMK_RAW_HID_REPORT_SIZE * MAX_QUEUED_MESSAGES];
static uint8_t message_queue_next_empty_offset = 0;

static bool block_buttons_on = false;
static bool block_pointer_on = false;
static bool block_wheel_on = false;
static bool send_buttons_on = false;
static bool send_pointer_on = false;
static bool send_wheel_on = false;
static bool control_state_changed = false;

static report_mouse_t accumulated_mouse_report = {0};

// ============================================================================
// MODULE API
// ============================================================================

void housekeeping_task_mouse_passthrough(void) {

    // we can only send one raw hid message per matrix scan, anything after the first message gets garbled for some reason
    if (message_queue_next_empty_offset != 0) {
        message_queue_next_empty_offset -= QMK_RAW_HID_REPORT_SIZE;
        raw_hid_send(message_queue + message_queue_next_empty_offset, QMK_RAW_HID_REPORT_SIZE);
    } else if (control_state_changed) {
        // send control payload
        memset(message_queue, 0, QMK_RAW_HID_REPORT_SIZE);
        message_queue[REPORT_OFFSET_COMMAND_ID] = RAW_HID_HUB_COMMAND_ID;
        message_queue[REPORT_OFFSET_DEVICE_ID] = device_id_remote;
        message_queue[REPORT_OFFSET_CONTROL_BLOCK_BUTTONS] = block_buttons_on ? 1 : 0;
        message_queue[REPORT_OFFSET_CONTROL_BLOCK_POINTER] = block_pointer_on ? 1 : 0;
        message_queue[REPORT_OFFSET_CONTROL_BLOCK_WHEEL] = block_wheel_on ? 1 : 0;
        message_queue[REPORT_OFFSET_CONTROL_SEND_BUTTONS] = send_buttons_on ? 1 : 0;
        message_queue[REPORT_OFFSET_CONTROL_SEND_POINTER] = send_pointer_on ? 1 : 0;
        message_queue[REPORT_OFFSET_CONTROL_SEND_WHEEL] = send_wheel_on ? 1 : 0;
        raw_hid_send(message_queue, QMK_RAW_HID_REPORT_SIZE);
        control_state_changed = false;
    }

    if (timer_elapsed32(last_connection_success_time) > HUB_CONNECTION_EXPIRY_INTERVAL) {
        state = MOUSE_PASSTHROUGH_DISCONNECTED;
        memset(&accumulated_mouse_report, 0, sizeof(accumulated_mouse_report));
    }

    if (timer_elapsed32(last_connection_attempt_time) > HUB_CONNECTION_ATTEMPT_INTERVAL) {
        last_connection_attempt_time = timer_read32();
        // send a registration report
        if (message_queue_next_empty_offset < sizeof(message_queue)) {
            memset(message_queue + message_queue_next_empty_offset, 0, QMK_RAW_HID_REPORT_SIZE);
            message_queue[message_queue_next_empty_offset + REPORT_OFFSET_COMMAND_ID] = RAW_HID_HUB_COMMAND_ID;
            message_queue[message_queue_next_empty_offset + REPORT_OFFSET_DEVICE_ID] = DEVICE_ID_HUB;
            message_queue[message_queue_next_empty_offset + REPORT_OFFSET_REGISTRATION] = 0x01;
            message_queue_next_empty_offset += QMK_RAW_HID_REPORT_SIZE;
        }
    }
}

void raw_hid_receive(uint8_t* data, uint8_t length) {

    if (data[REPORT_OFFSET_COMMAND_ID] != RAW_HID_HUB_COMMAND_ID) {
        return;
    }

    last_connection_success_time = timer_read32();
    if (state == MOUSE_PASSTHROUGH_DISCONNECTED) {
        state = MOUSE_PASSTHROUGH_HUB_CONNECTED;
    }

    if (state == MOUSE_PASSTHROUGH_REMOTE_CONNECTED && data[REPORT_OFFSET_DEVICE_ID] == device_id_remote) {
        // unpack data payload
        accumulated_mouse_report.buttons = data[REPORT_OFFSET_DATA_BUTTONS];
        accumulated_mouse_report.x += ((uint16_t)data[REPORT_OFFSET_DATA_X_MSB] << 8) | ((uint16_t)data[REPORT_OFFSET_DATA_X_LSB]);
        accumulated_mouse_report.y += ((uint16_t)data[REPORT_OFFSET_DATA_Y_MSB] << 8) | ((uint16_t)data[REPORT_OFFSET_DATA_Y_LSB]);
        accumulated_mouse_report.v += ((uint16_t)data[REPORT_OFFSET_DATA_V_MSB] << 8) | ((uint16_t)data[REPORT_OFFSET_DATA_V_LSB]);
        accumulated_mouse_report.h += ((uint16_t)data[REPORT_OFFSET_DATA_H_MSB] << 8) | ((uint16_t)data[REPORT_OFFSET_DATA_H_LSB]);

    } else if (data[REPORT_OFFSET_DEVICE_ID] == DEVICE_ID_HUB) {
        if (data[REPORT_OFFSET_DEVICE_ID_SELF] == DEVICE_ID_UNASSIGNED) {
            // hub has shutdown
            state = MOUSE_PASSTHROUGH_DISCONNECTED;
            memset(&accumulated_mouse_report, 0, sizeof(accumulated_mouse_report));
        } else {
            device_id_self = data[REPORT_OFFSET_DEVICE_ID_SELF];
            memcpy(device_id_others, data + REPORT_OFFSET_DEVICE_ID_OTHERS, MAX_REGISTERED_DEVICES - 1);
            if (state == MOUSE_PASSTHROUGH_REMOTE_CONNECTED) {
                // see if we're still connected
                bool found = false;
                for (int i = REPORT_OFFSET_DEVICE_ID_OTHERS; i < QMK_RAW_HID_REPORT_SIZE; i++) {
                    if (data[i] == device_id_remote) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    state = MOUSE_PASSTHROUGH_HUB_CONNECTED;
                    memset(&accumulated_mouse_report, 0, sizeof(accumulated_mouse_report));
                }
            }
        }
        
    } else if (state == MOUSE_PASSTHROUGH_HUB_CONNECTED && data[REPORT_OFFSET_HANDSHAKE] == 13) {
        // handshake step 2/4: all capable keyboards respond to mouse
        if (message_queue_next_empty_offset < sizeof(message_queue)) {
            device_id_remote = data[REPORT_OFFSET_DEVICE_ID];
            memset(message_queue + message_queue_next_empty_offset, 0, QMK_RAW_HID_REPORT_SIZE);
            message_queue[message_queue_next_empty_offset + REPORT_OFFSET_COMMAND_ID] = RAW_HID_HUB_COMMAND_ID;
            message_queue[message_queue_next_empty_offset + REPORT_OFFSET_DEVICE_ID] = device_id_remote;
            message_queue[message_queue_next_empty_offset + REPORT_OFFSET_HANDSHAKE] = 26;
            message_queue_next_empty_offset += QMK_RAW_HID_REPORT_SIZE;
        }

    } else if (state == MOUSE_PASSTHROUGH_HUB_CONNECTED && data[REPORT_OFFSET_DEVICE_ID] == device_id_remote && data[REPORT_OFFSET_HANDSHAKE] == 39) {
        // handshake step 4/4: keyboard silently receives mouse response
        state = MOUSE_PASSTHROUGH_REMOTE_CONNECTED;
        device_id_remote = data[REPORT_OFFSET_DEVICE_ID];
    }
}

report_mouse_t pointing_device_driver_get_report(report_mouse_t mouse_report) {
    memcpy(&mouse_report, &accumulated_mouse_report, sizeof(accumulated_mouse_report));
    memset(&accumulated_mouse_report, 0, sizeof(accumulated_mouse_report));
    accumulated_mouse_report.buttons = mouse_report.buttons;
#    ifdef POINTING_DEVICE_HIRES_SCROLL_ENABLE
    mouse_report.h *= pointing_device_get_hires_scroll_resolution();
    mouse_report.v *= pointing_device_get_hires_scroll_resolution();
#    endif
    return mouse_report;
}

// ============================================================================
// USER API
// ============================================================================

bool is_mouse_passthrough_connected(void) {
    return state == MOUSE_PASSTHROUGH_REMOTE_CONNECTED;
}

void mouse_passthrough_set_buttons_state(bool send, bool block) {
    if (send_buttons_on != send || block_buttons_on != block) {
        send_buttons_on = send;
        block_buttons_on = block;
        control_state_changed = true;
    }
}

void mouse_passthrough_set_pointer_state(bool send, bool block) {
    if (send_pointer_on != send || block_pointer_on != block) {
        send_pointer_on = send;
        block_pointer_on = block;
        control_state_changed = true;
    }
}

void mouse_passthrough_set_wheel_state(bool send, bool block) {
    if (send_wheel_on != send || block_wheel_on != block) {
        send_wheel_on = send;
        block_wheel_on = block;
        control_state_changed = true;
    }
}

void mouse_passthrough_send_reset_command(void) {
    // send a registration report
    if (message_queue_next_empty_offset < sizeof(message_queue)) {
        memset(message_queue + message_queue_next_empty_offset, 0, QMK_RAW_HID_REPORT_SIZE);
        message_queue[message_queue_next_empty_offset + REPORT_OFFSET_COMMAND_ID] = RAW_HID_HUB_COMMAND_ID;
        message_queue[message_queue_next_empty_offset + REPORT_OFFSET_DEVICE_ID] = device_id_remote;
        message_queue[message_queue_next_empty_offset + REPORT_OFFSET_RESET] = 0x01;
        message_queue_next_empty_offset += QMK_RAW_HID_REPORT_SIZE;
    }
}

#endif  // MOUSE_PASSTHROUGH_RECEIVER