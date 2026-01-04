// Copyright 2026 Morgan Newell Sun
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once
#include <stdint.h>

void simple_timer_on(uint32_t delay_ms);
void simple_timer_off(void);
void simple_timer_callback(void);
