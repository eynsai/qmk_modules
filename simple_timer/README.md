# `simple_timer`

This module provides a wrapper for QMK's deferred execution system. The focus is on making it easier to use and reason about, at the cost of sacrificing flexibility and power.

Calling `simple_timer_on` will trigger the deferred execution of the user-defined `simple_timer_callback` after a specified delay.
Calling `simple_timer_on` before the callback executes will reset/modify the delay, while calling `simple_timer_off` will cancel the execution.

This module also gracefully handles cases where `simple_timer_on` and `simple_timer_off` are called *inside* `simple_timer_callback`.
