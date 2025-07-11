# High-Resolution Dragscroll Module for QMK

This module provides high-resolution drag scrolling.
Some simple keycodes are provided to get you started. 
More advanced control can be achieved through a handful of user API functions.

## Premade Keycodes

Include the following keycodes in your keymap for basic activation and control:

- `KC_HIRES_DRAGSCROLL_MO` (`KC_DSMO`): Momentarily enable dragscrolling while held.
- `KC_HIRES_DRAGSCROLL_TG` (`KC_DSTG`): Toggle dragscrolling on or off.

## Configuration Defines

There are several parameters to control the way drag-scroll behaves and feels.
The most useful parameters to know about are:

| Define                                | Default | Description                                                 |
| ------------------------------------- | ------- | ----------------------------------------------------------- |
| `HIRES_DRAGSCROLL_MULTIPLIER_H`       | `8.0`   | Horizontal sensitivity. Negative to invert direction.       |
| `HIRES_DRAGSCROLL_MULTIPLIER_V`       | `8.0`   | Vertical sensitivity. Negative to invert direction.         |
| `HIRES_DRAGSCROLL_SMOOTHING`          | defined | `#undefine` this to disable smoothing.                      |
| `HIRES_DRAGSCROLL_SMOOTHING_AMOUNT`   | `5`     | Number of samples used for smoothing. Must be at least `1`. |
| `HIRES_DRAGSCROLL_ACCELERATION`       | defined | `#undefine` this to disable acceleration.                   |
| `HIRES_DRAGSCROLL_ACCELERATION_SCALE` | `500.0` | Scaling factor for acceleration.                            |

There are some additional parameters which you probably don't have to adjust:

| Define                                     | Default    | Description                                                                        |
| ------------------------------------------ | ---------- | ---------------------------------------------------------------------------------- |
| `HIRES_DRAGSCROLL_THROTTLE_MS`             | `16`       | Minimum interval between scroll reports (to avoid the host computer freaking out). |
| `HIRES_DRAGSCROLL_TIMEOUT_MS`              | `500`      | Time after which the dragscroll state resets if no movement occurs.                |
| `HIRES_DRAGSCROLL_AXIS_SNAPPING_THRESHOLD` | `0.25`     | Controls axis snapping. Hard to explain - read the code.                           |
| `HIRES_DRAGSCROLL_AXIS_SNAPPING_RATIO`     | `2.0`      | Controls axis snapping. Hard to explain - read the code.                           |
| `HIRES_DRAGSCROLL_ACCELERATION_BLEND`      | `0.872116` | Blend factor for acceleration curve shaping.                                       |

## User API

### API Functions

Calling `hires_dragscroll_on()` will turn on dragscrolling, and `hires_dragscroll_off()` will turn off dragscrolling.
You can check whether or not dragscrolling is on by calling `is_hires_dragscroll_on()`.

When dragscroll is turned on using `hires_dragscroll_on()`, vertical motion will result in VWHEEL reports, and horizontal motion will result in HWHEEL reports.
However, some software might require an alternative form of scrolling input.
For example, some softwares might not respond to HWHEEL for horizontal scrolling, and instead require SHIFT + VWHEEL.
To deal with such cases, you can use `hires_dragscroll_on_with_config(config)` instead, where `config` is defined as follows:

```c
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
```

In most use cases, you'll probably want/need angle snapping when using high resolution dragscroll.
However, in certain specialized software (e.g. for art/design), you might not want it.
In these cases, use `hires_dragscroll_on_without_axis_snapping()`.

### Hook Functions

The following weak functions can be overridden to customize processing:

```c
report_mouse_t pre_hires_dragscroll_accumulate_task_user(report_mouse_t report);
report_mouse_t pre_hires_dragscroll_scroll_task_user(report_mouse_t report);
report_mouse_t post_hires_dragscroll_scroll_task_user(report_mouse_t report);
```

These functions allow you to process the mouse report before/after dragscroll is applied.
