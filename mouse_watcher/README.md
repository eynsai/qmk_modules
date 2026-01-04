# `mouse_watcher`

This module provides a way to trigger a callback whenever the pointing device moves more than a certain amount.

Once `mouse_watcher_on` is called, the module will begin accumulating pointer movement.
If the pointer moves more than the specified amount, the user-defined `mouse_watcher_callback` will be executed, and the mouse watcher will automatically be turned off.
The mouse watcher can also be turned off by calling `mouse_watcher_off` before the callback executes.
