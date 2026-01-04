# `mouse_buffer`

This module aims to solve a very specific problem: 
I wanted a "lazy mouse modifiers" feature where QMK "reacts" to a mouse click by registering certain keyboard modifiers, resulting in the modifiers being applied to the mouse click.
However, QMK doesn't use the same mechanisms and endpoints to send keyboard and mouse inputs.
This means that, with a naive implementation of this feature, the host PC receives and parses the mouse input before the keyboard modifier, resulting in an un-modified click.

This module implements a system which, when activated, blocks and accumulates all button and wheel inputs for a brief duration before releasing them.
This slight delay is enough time for the keyboard modifiers to be registered and parsed by the host PC.
