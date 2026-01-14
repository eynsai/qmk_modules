# `mouse_passthrough`

This module implements a system for using raw HID messages, in conjunction with software running on the host PC, to use one QMK pointing device (the "sender") as a pointing device on another QMK device (the "receiver").
The host PC software can be found [here](https://github.com/eynsai/raw-hid-hub). (Warning: very fragile code.)

The receiver and sender both need to use this module, and must `#define MOUSE_PASSTHROUGH_RECEIVER` and `#define MOUSE_PASSTHROUGH_SENDER`, respectively.
Upon both devices being connected to the same host PC, they will automatically find each other via a handshake procedure.
After the handshake, the receiver can issue commands to control the sender's behavior.

By default, the sender behaves as a normal QMK device would, and sends all pointing device input, including buttons, wheel, and pointer, as mouse reports to the host PC.
However, the receiver can tell the sender to send some or all of these components to the receiver device as raw HID messages instead.
Any messages sent to the receiver will be parsed into mouse reports and processed by the receiver-side QMK code, effectively allowing the receiver device to "take over" as the pointing device.
