// Copyright 2025 Morgan Newell Sun
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#ifndef POINTING_DEVICE_ENABLE
#    error "Must enable the pointing device feature for this module to work"
#endif

#ifndef MOUSE_AXIS_SNAPPING_THRESHOLD
#    define MOUSE_AXIS_SNAPPING_THRESHOLD 25.0
#endif

#ifndef MOUSE_AXIS_SNAPPING_RATIO
#    define MOUSE_AXIS_SNAPPING_RATIO 2.0
#endif
