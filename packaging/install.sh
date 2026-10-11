#!/usr/bin/env bash
# Steam discovery adapted from EDF6Coop; see EDF6Coop_LICENSE.txt.
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
uninstall=0
if [ "${1:-}" = --uninstall ]; then uninstall=1; shift; fi
find_game() {
    for d in "$here" "$(dirname "$here")"; do
        if [ -f "$d/EDF6.exe" ]; then echo "$d"; return; fi
    done
    for steam in "$HOME/.local/share/Steam" "$HOME/.steam/steam" "$HOME/.var/app/com.valvesoftware.Steam/.local/share/Steam"; do
        libs=$steam
        vdf=$steam/steamapps/libraryfolders.vdf
        if [ -f "$vdf" ]; then
            libs="$libs"$'\n'"$(sed -n 's/^[[:space:]]*"path"[[:space:]]*"\(.*\)"/\1/p' "$vdf")"
        fi
        while IFS= read -r lib; do
            d="$lib/steamapps/common/EARTH DEFENSE FORCE 6"
            if [ -f "$d/EDF6.exe" ]; then echo "$d"; return; fi
        done <<< "$libs"
    done
    return 0
}
game=${1:-$(find_game)}
if [ -z "$game" ] || [ ! -f "$game/EDF6.exe" ]; then
    echo 'Game not found. Run: bash install.sh "/path/to/EARTH DEFENSE FORCE 6"' >&2
    exit 1
fi
if pgrep -ix 'EDF6.exe' >/dev/null; then echo 'Quit EDF6 first.' >&2; exit 1; fi
target=$game/Mods/Plugins/EDF6AutoLoot.dll
backup_existing() {
    backup=$(mktemp -d "$game/AutoLoot-backup-XXXXXXXX")
    cp -p -- "$target" "$backup/EDF6AutoLoot.dll"
    echo "Saved previous DLL: $backup/EDF6AutoLoot.dll"
}
if [ "$uninstall" = 1 ]; then
    if [ -f "$target" ]; then backup_existing; rm -- "$target"; fi
    echo 'AutoLoot removed. Backups, logs, loader and other mods remain.'
    exit 0
fi
if [ ! -f "$game/winmm.dll" ]; then
    echo 'Install EDFModLoader first. AutoLoot does not include a loader.' >&2; exit 1
fi
if [ -f "$game/Lootmaster Readme.txt" ]; then
    echo 'Remove the old Lootmaster installation using its saved manifest before installing AutoLoot.' >&2; exit 1
fi
source=$here/EDF6AutoLoot.dll
if [ ! -f "$source" ]; then echo 'Extract the whole release ZIP first.' >&2; exit 1; fi
mkdir -p -- "$(dirname "$target")"
if [ -f "$target" ] && cmp -s -- "$source" "$target"; then
    echo 'This AutoLoot DLL is already installed.'; exit 0
fi
if [ -e "$target" ]; then backup_existing; fi
stage=$(mktemp "$(dirname "$target")/.autoloot-XXXXXXXX")
trap 'rm -f -- "$stage"' EXIT
cp -- "$source" "$stage"
mv -f -- "$stage" "$target"
echo 'Installed AutoLoot (offline prototype). Back up saves before testing.'
echo 'Keep existing Steam options. Proton requires: WINEDLLOVERRIDES="winmm=n,b" %command%'
