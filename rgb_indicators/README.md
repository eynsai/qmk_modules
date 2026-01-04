# `rgb_indicators`

This module provides a simplified way of defining and triggering rgblight (i.e. no per-key lighting) animations.

These animations are intended to be utilitarian ways of indicating both persistent and transient information.

Persistent information is represented as "states". 
A state consists of a "breathing" pattern that loops between two colors at some frequency, or a single solid color.

Transient information is represented as "transitions".
Transitions interpolate from the current state to an accent color, linger on that color for a bit, then interpolate to a target state.

Note that states are *not* graph nodes, and transitions are *not* graph edges. Any transition can be used to move from any initial state to any target state.
This module gracefully handles edge cases involving interrupting ongoing transitions. 
It also tries to select the simplest possible interpolation paths in HSV space to avoid weird hue rotations.

Example usage:

```
// defining states and transitions
typedef enum indicator_state_t {
    MY_INDICATOR_STATE = 0,
    MY_OTHER_INDICATOR_STATE,
    ...
} indicator_state_t;

typedef enum indicator_transition_t {
    MY_INDICATOR_TRANSITION = 0,
    ...
} indicator_transition_t;

const rgb_indicator_state_t rgb_indicator_states[] = {
    [MY_INDICATOR_STATE] = { .breathing = true, .color_a = HSV_BLUE, .color_b = HSV_GREEN, .period_ms = 4000 },
    [MY_OTHER_INDICATOR_STATE] = { .breathing = true, .color_a = HSV_YELLOW, .color_b = HSV_RED, .period_ms = 4000 },
    ...
};
const rgb_indicator_transition_t rgb_indicator_transitions[] = {
    [MY_INDICATOR_TRANSITION] = { .accent_color = HSV_PURPLE, .fade_in_ms = 50, .hold_ms = 50, .fade_out_ms = 500 },
    ...
};

// triggering a transition from the current state to another state
rgb_indicators_start_transition(MY_INDICATOR_TRANSITION, MY_OTHER_INDICATOR_STATE);
```

