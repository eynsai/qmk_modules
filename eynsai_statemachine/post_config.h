// Copyright 2025 Morgan Newell Sun
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SUPERCTRL_TAPPING_TERM
#    define SUPERCTRL_TAPPING_TERM 175
#endif
#ifndef SUPERALT_TAPPING_TERM
#    define SUPERALT_TAPPING_TERM 175
#endif
#ifndef SUPERGUI_TAPPING_TERM
#    define SUPERGUI_TAPPING_TERM 175
#endif
#ifndef BASE_TAPPING_TERM
#    define BASE_TAPPING_TERM 800
#endif
#ifndef BITWIG_TAPPING_TERM
#    define BITWIG_TAPPING_TERM 2500
#endif

#ifndef DRAGSCROLL_DETECTION_DEADZONE
#    define DRAGSCROLL_DETECTION_DEADZONE 50
#endif

#ifndef MOUSE_BUFFER_DURATION
#    define MOUSE_BUFFER_DURATION 50
#endif

#ifndef COLOR_NEUTRAL
#    define COLOR_NEUTRAL {0, 0, RGBLIGHT_LIMIT_VAL}
#endif
#ifndef COLOR_ONESHOT_A
#    define COLOR_ONESHOT_A {105, 255, RGBLIGHT_LIMIT_VAL}
#endif
#ifndef COLOR_ONESHOT_MIDPOINT
#    define COLOR_ONESHOT_MIDPOINT {115, 255, RGBLIGHT_LIMIT_VAL}
#endif
#ifndef COLOR_ONESHOT_B
#    define COLOR_ONESHOT_B {125, 255, RGBLIGHT_LIMIT_VAL}
#endif
#ifndef COLOR_CTRL
#    define COLOR_CTRL {170, 255, RGBLIGHT_LIMIT_VAL}
#endif
#ifndef COLOR_ALT
#    define COLOR_ALT {80, 255, RGBLIGHT_LIMIT_VAL}
#endif
#ifndef COLOR_GUI
#    define COLOR_GUI COLOR_NEUTRAL
#endif
#ifndef COLOR_QWER
#    define COLOR_QWER {15, 255, RGBLIGHT_LIMIT_VAL}
#endif
#ifndef COLOR_GAME
#    define COLOR_GAME {190, 255, RGBLIGHT_LIMIT_VAL}
#endif
#ifndef COLOR_BITWIG
#    define COLOR_BITWIG {8, 255, RGBLIGHT_LIMIT_VAL}
#endif
#ifndef TIMING_TO
#    define TIMING_TO .fade_in_ms = 50, .hold_ms = 50, .fade_out_ms = 500
#endif
#ifndef TIMING_FROM
#    define TIMING_FROM .fade_in_ms = 50, .hold_ms = 0, .fade_out_ms = 100
#endif
#ifndef TIMING_FLASH
#    define TIMING_FLASH .fade_in_ms = 200, .hold_ms = 300, .fade_out_ms = 500
#endif