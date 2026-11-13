# Breath of Fire IV on Linux (Steam / Proton) — input and audio fixes

Fixes for the Steam release of **Breath of Fire IV** (appid `4249150`, the GOG build with GOG's
DirectX/input wrappers) running under Proton.

| Symptom | Cause | Fix |
|---|---|---|
| "PRESS ENTER BUTTON" does nothing; no Start/Select | Start (Enter) and Select (F1) only exist through GOG's `dinput.dll` wrapper, but Wine loads its builtin `dinput` instead | DLL override `dinput=n,b` |
| Keyboard dies after switching to another Windows program running in the same Proton session (a trainer, a launcher, ...) and back | The game acquires its DirectInput devices once with `DISCL_FOREGROUND` and never again. Wine drops the acquisition when another Wine window becomes foreground; every later `GetDeviceState` returns `DIERR_NOTACQUIRED` | `version.dll` proxy from this repo: re-acquires on `DIERR_NOTACQUIRED` / `DIERR_INPUTLOST` |
| Bursts of noise, or all sound dying, when the default output is 7.1 | Wine's DirectSound mixer has no 8-channel support (`fixme:dsound:DSOUND_MixerVol There is no support for 8 channels`) and emits uninitialised samples | Send the game to a stereo sink (`PULSE_SINK=...`) |

## Status

Tested on **one machine only**: Arch Linux, KDE Plasma (Wayland), GE-Proton11-12, PipeWire, keyboard
input. Not tested with Valve Proton, on Steam Deck, or with the standalone GOG release (the DLL
should work there too, since the game files are the same, but that is unverified).

## Install

```sh
./install.sh            # finds the game in your Steam libraries
./install.sh /path/to/steamapps/common/4249150_BreathofFire4
```

This copies `dist/version.dll` next to `BOF4.exe` (`english/` and `japanese/`) and, for GE-Proton,
installs `protonfix/4249150.py` to `~/.config/protonfixes/localfixes/`. It does not modify any
existing game file. To uninstall, delete those `version.dll` files and the protonfix.

**GE-Proton:** nothing else to do.

**Valve Proton** (no protonfixes): set the Steam launch options to

```
WINEDLLOVERRIDES="ddraw=n,b;dinput=n,b;version=n,b" %command%
```

Type the quotes as plain `"` — typographic quotes pasted from a chat or web page silently break it.

### 7.1 / surround default output

If your default sink has 8 channels, pick a stereo sink (`pactl list short sinks`) and add it:

- GE-Proton: `BOF4_STEREO_SINK=<sink name> %command%`
- Valve Proton: `PULSE_SINK=<sink name> WINEDLLOVERRIDES="ddraw=n,b;dinput=n,b;version=n,b" %command%`

## Controls (keyboard)

Enter = Start, F1 = Select, arrow keys, and Z / X / C / V / A / D / Tab for the pad buttons
(the last group is stored in `bof4.cfg`).

## How the DLL works

`BOF4.exe` imports `version.dll`, so a DLL with that name in the game folder is loaded into the
process. It forwards all `version.dll` exports to the real one, then patches the
`GetDeviceState` / `GetDeviceData` slots of Wine's `IDirectInputDevice7A` vtable: when a call fails
with "not acquired" or "input lost", it calls `Acquire()` and retries once. While the game is not
the foreground window `Acquire()` keeps failing, so no input leaks in from other windows.

## Build

```sh
./build.sh
```

Needs `winegcc` with i386 PE support (Wine's own toolchain with clang/lld; mingw is not required).
The prebuilt `dist/version.dll` is built from `src/` with exactly this script.

## License

MIT. Not affiliated with Capcom, GOG or Valve. No game files are included.
