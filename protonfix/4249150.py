"""GE-Proton local fix for Breath of Fire IV (Steam appid 4249150).

Install as ~/.config/protonfixes/localfixes/4249150.py (GE-Proton / umu only;
Valve Proton does not read protonfixes — use the launch options from the README).
"""

import os

from protonfixes import util


def main() -> None:
    # The Steam release is the GOG build: its ddraw.dll / dinput.dll wrappers
    # must load instead of Wine's builtins (Start/Select only exist through
    # GOG's dinput.dll).
    util.winedll_override('ddraw', util.OverrideOrder.NATIVE_BUILTIN)
    util.winedll_override('dinput', util.OverrideOrder.NATIVE_BUILTIN)
    # version.dll proxy from this project: re-acquires DirectInput devices
    # after Wine drops them on focus changes.
    util.winedll_override('version', util.OverrideOrder.NATIVE_BUILTIN)

    # Optional: Wine's DirectSound cannot mix into an 8-channel (7.1) device.
    # If your default output is 7.1, name a stereo sink here via the launch
    # options, e.g.  BOF4_STEREO_SINK=<sink name from `pactl list short sinks`> %command%
    sink = os.environ.get('BOF4_STEREO_SINK')
    if sink and not os.environ.get('PULSE_SINK'):
        util.set_environment('PULSE_SINK', sink)
