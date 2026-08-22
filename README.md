# CEC-Knob-Windows
A small tray app that installs a windows hook to redirect volume control to CEC commands via `libcec`.
It uses a dual-threaded design to ensure the windows keyboard hook does not slow the system down.

Note: The CEC functionality is currently untested as I do not have a CEC injector.

Note: `winMain.c` is triple licensed to allow you to use program as a template for a try application without requiring adopting the GPL.
`winMain.c` does not actually contain any GPL code, only the compiled binary so you do not have to worry licensing issues.
