#!/bin/bash
# Install the Breath of Fire IV (Steam 4249150) Linux fixes.
#   ./install.sh [path to the game folder]
# Without an argument the game is looked up in the Steam libraries.
set -e
here="$(cd "$(dirname "$0")" && pwd)"
dll="$here/dist/version.dll"
[ -f "$dll" ] || { echo "dist/version.dll is missing - run ./build.sh first" >&2; exit 1; }

game="$1"
if [ -z "$game" ]; then
	for vdf in "$HOME/.steam/steam/steamapps/libraryfolders.vdf" \
		"$HOME/.local/share/Steam/steamapps/libraryfolders.vdf" \
		"$HOME/.var/app/com.valvesoftware.Steam/.local/share/Steam/steamapps/libraryfolders.vdf"; do
		[ -f "$vdf" ] || continue
		while IFS= read -r lib; do
			[ -d "$lib/steamapps/common/4249150_BreathofFire4" ] && { game="$lib/steamapps/common/4249150_BreathofFire4"; break 2; }
		done < <(sed -n 's/^[[:space:]]*"path"[[:space:]]*"\(.*\)"$/\1/p' "$vdf")
	done
fi
[ -d "$game" ] || { echo "Game folder not found. Pass it as an argument." >&2; exit 1; }

n=0
for lang in english japanese; do
	[ -f "$game/$lang/BOF4.exe" ] || continue
	cp "$dll" "$game/$lang/version.dll"
	echo "installed $game/$lang/version.dll"
	n=$((n + 1))
done
[ "$n" -gt 0 ] || { echo "No BOF4.exe under $game/english or $game/japanese." >&2; exit 1; }

fixdir="${XDG_CONFIG_HOME:-$HOME/.config}/protonfixes/localfixes"
if [ -e "$fixdir/4249150.py" ] && ! cmp -s "$here/protonfix/4249150.py" "$fixdir/4249150.py"; then
	echo "kept existing $fixdir/4249150.py (differs from this project's copy)"
else
	mkdir -p "$fixdir"
	cp "$here/protonfix/4249150.py" "$fixdir/4249150.py"
	echo "installed $fixdir/4249150.py (used by GE-Proton only)"
fi
echo "Done. With Valve Proton, also set the launch options from the README."
