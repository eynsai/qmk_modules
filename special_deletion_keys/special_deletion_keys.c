#include QMK_KEYBOARD_H
#include "quantum.h"

// ============================================================================
// MODULE API
// ============================================================================

bool process_record_special_deletion_keys(uint16_t keycode, keyrecord_t *record) {

    switch(keycode) {

        // delete-word keys
        case KC_DWL:
            if (record->event.pressed) {
                tap_code16(C(KC_BSPC));
            }
            return false;
        case KC_DWR:
            if (record->event.pressed) {
                tap_code16(C(KC_DEL));
            }
            return false;

        // delete-word key
        case KC_DLN:
            if (record->event.pressed) {
                // tap_code(KC_MINUS);  // at one point I thought this was necessary - why?
                tap_code(KC_END);
                tap_code(KC_END);
                tap_code16(S(KC_HOME));
                tap_code16(S(KC_HOME));
                tap_code16(S(KC_LEFT));
                tap_code(KC_BSPC);
            }
            return false;
        
        // everything else
        default:
            return true;
    }
}

