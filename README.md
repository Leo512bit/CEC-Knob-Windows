# CEC-Knob-Windows
A small tray app that installs a windows hook to redirect volume control to CEC commands via `libcec`.
It uses a dual-threaded design to ensure the windows keyboard hook does not slow the system down.


## How to use
The program requires `cec.dll` (and maybe some other stuff in libcec, I'm honestly unsure as I have no means to test the CEC capability of this program but I do know that it only requires `cec.dll` to launch).
Install `libcec`.

The default location: `C:\Program Files\Pulse-Eight\USB-CEC Adapter` is recommended but not required as if `cec.dll` cannot be found in that location the program automatically looks in its own folder.
However you do not have to put this program inside your `libcec` installation as you can pass the path of `cec.dll` as an argument.

Ultimately the only user interface is in the tray to exit the application. While the program is open it just cancels out Windows volume keys and sends out CEC commands to a CEC device.

## Building

Install `libcec` in the default location: `C:\Program Files\Pulse-Eight\USB-CEC Adapter`. Open `CEC Knob.slnx` and build the solution.

If you desire you can grab the headers from `libcec` and configure the project to point to the location where you havee them. Installing `libcec` is not required to build the program.
## Other information
> [!IMPORTANT]
> The CEC functionality of this program is **untested as I have no means to test it as I have no access to CEC hardware.**

> [!TIP]
> [winMain.c](./CEC%20Knob/winMain.c) is triple licensed to allow you to use program as a template for a tray application without requiring adopting the GPL.
> Even though the project as a whole is licensed under the GPLv3 due to the linking to `libcec`, that explicit source file can be licensed under any license that is listed in `winMain.c`.
> This is because `winMain.c` by itself doesn't inherently link to any GPL code.
