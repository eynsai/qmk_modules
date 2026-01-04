// Copyright 2026 Morgan Newell Sun
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once
#include <stdint.h>

void mouse_watcher_on(uint16_t threshold);
void mouse_watcher_off(void);
void mouse_watcher_callback(void);
