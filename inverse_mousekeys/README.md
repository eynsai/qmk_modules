# `inverse_mousekeys`

Typically, (module/keyboard/user)-level QMK code must process keystrokes using `process_record_*` and pointing device inputs with `pointing_device_task_*`.
This module is a hack for creating a single unified API for both (as long as you don't need to deal with pointer movement).
It intercepts mouse reports and constructs dummy keycodes/records in response to mouse button and wheel inputs, which are then passed on to `process_record_*`.
Just like with normal keycodes, (module/keyboard/user)-level `process_record_*` can return `true` or `false` to indicate whether or not to block these inputs from downstream processing.
