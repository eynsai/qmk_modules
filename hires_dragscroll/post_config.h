// Copyright 2025 Morgan Newell Sun
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#ifndef POINTING_DEVICE_ENABLE
#    error "Must enable the pointing device feature for this module to work"
#endif

#ifndef HIRES_DRAGSCROLL_MULTIPLIER_H
#    define HIRES_DRAGSCROLL_MULTIPLIER_H 0.3
#endif

#ifndef HIRES_DRAGSCROLL_MULTIPLIER_V
#    define HIRES_DRAGSCROLL_MULTIPLIER_V -0.3
#endif

#ifndef HIRES_DRAGSCROLL_THROTTLE_MS
#    define HIRES_DRAGSCROLL_THROTTLE_MS 16
#endif

#ifndef HIRES_DRAGSCROLL_TIMEOUT_MS
#    define HIRES_DRAGSCROLL_TIMEOUT_MS 500
#endif

#ifndef HIRES_DRAGSCROLL_AXIS_SNAPPING_THRESHOLD
#    define HIRES_DRAGSCROLL_AXIS_SNAPPING_THRESHOLD 0.25
#endif

#ifndef HIRES_DRAGSCROLL_AXIS_SNAPPING_RATIO
#    define HIRES_DRAGSCROLL_AXIS_SNAPPING_RATIO 2.0
#endif

#ifndef HIRES_DRAGSCROLL_SMOOTHING_AMOUNT
#    define HIRES_DRAGSCROLL_SMOOTHING_AMOUNT 5
#endif

#ifndef HIRES_DRAGSCROLL_ACCELERATION_SCALE
#    define HIRES_DRAGSCROLL_ACCELERATION_SCALE 500.0
#endif

#ifndef HIRES_DRAGSCROLL_ACCELERATION_BLEND
#    define HIRES_DRAGSCROLL_ACCELERATION_BLEND 0.872116
#endif