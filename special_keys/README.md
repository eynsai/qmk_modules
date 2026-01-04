# `special_keys`

This module contains implementations of some miscellaneous custom keycodes that don't warrant their own modules.

- `KC_DELETE_LINE`: Sends a sequence of keycodes that deletes the current line of text.
- `KC_VSCODE_ADD_CURSOR`: Uses OS detection to send the appropriate arrow modifiers for "Add Cursor Above" and "Add Cursor Below" in VSCode.
- `KC_CONDITIONAL_CTRL`: A CTRL key that only modifies the left and right arrow keys. This makes it a lot easier to use arrow keys to move around by words.
- `KC_LAZY_ALT`: An ALT key that only registers once you press an arrow key as well. This makes it harder to accidently move focus to toolbars or context menus.