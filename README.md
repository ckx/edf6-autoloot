# EDF6 AutoLoot

An EDFModLoader plugin for EARTH DEFENSE FORCE 6 that collects remaining weapon and
armor crates when a mission is cleared.

Status: offline prototype. Built and tested against the game's code under Wine; not
yet played in a live mission.

## Behavior

- When a mission is cleared offline, every weapon and armor crate still on the field
  is credited through the game's own pickup function and removed, as if a player had
  walked over it. This happens at the end of the clear fade, before the results.
- Players are not moved and loadouts are not changed. Health pickups stay.
- The game's drop rates, its 1024-pickup limit, weapon rolls and result handling stay
  as they are. Failure, retreat and retry collect nothing.
- Online sessions collect nothing until reward authority is verified.
- Only the supported EDF.dll (Steam, 2025-01 update) is patched. Any other build, or
  code another mod changed, makes the plugin refuse and change nothing.

`Mods/Plugins/EDF6AutoLoot.log` records whether the plugin installed, and one line
per cleared mission with the crates taken and the counts the game added.

## Documents

- `docs/design.md`: behavior, runtime design and unresolved decisions.
- `docs/research.md`: the game interfaces used and their evidence.
- `docs/implementation-plan.md`: milestones and acceptance checks.
- `AGENTS.md`: working rules for contributors and coding agents.

## Build

Linux, cross-compiling with clang for MinGW-w64 and testing under Wine:

```sh
EDF6_GAME_DIR="/path/to/EARTH DEFENSE FORCE 6" ./build.sh -t
```

The plugin lands in `out/EDF6AutoLoot.dll`. Fedora needs `clang lld mingw64-gcc-c++
mingw64-winpthreads-static wine cmake ninja-build`. `EDF6_GAME_DIR` lets the tests
run the game's own code from `EDF.dll`, which is only read. Without it those tests
report skipped, and `-r` makes the configure fail instead.

Windows requires Visual Studio 2022 with Desktop development with C++ and CMake:

```powershell
./build.ps1 -Test
```

`-RequireGame` requires EDF.dll. Build and create a release ZIP plus SHA-256 file
with `python tools/package.py`. This rebuilds and runs the native tests first.
Run installer fixtures with `python -m unittest discover -s tests -p installer_test.py`.
CI builds with MSVC; tests requiring the commercial game report skipped there.

## Installation

Download the ZIP from Releases and extract it into a separate folder:
https://github.com/ckx/edf6-autoloot/releases

This is an experimental offline preview. Close EDF6 and back up your saves before
testing. EDFModLoader must already be installed, for example by the co-op mod.
AutoLoot has no runtime dependency on the co-op plugin. It does not bundle a loader.
The loader project is at:
https://github.com/BlueAmulet/EDFModLoader

Remove old Lootmaster overrides using your saved installation manifest first.
The installer refuses when its readme is present, but cannot detect all remnants.
It never removes Lootmaster files automatically.

- Windows: double-click `INSTALL.bat`. It finds Steam or asks for the game folder.
- Linux/Steam Deck: run `bash install.sh` in the extracted folder.
- Explicit folder: `INSTALL.bat -GameDir "D:\SteamLibrary\steamapps\common\EARTH DEFENSE FORCE 6"`
  or `bash install.sh "/path/to/EARTH DEFENSE FORCE 6"`.

The installer copies only EDF6AutoLoot.dll into Mods/Plugins. Existing loaders,
settings and other mods remain. Proton requires this Steam launch option; preserve
any other options you already use:

```text
WINEDLLOVERRIDES="winmm=n,b" %command%
```

Run `UNINSTALL.bat` or `bash install.sh --uninstall` to remove the plugin. Both accept
the same game-folder argument as installation. Logs remain. Before updates or
removal, the installer saves the existing DLL in a unique `AutoLoot-backup-*` folder
beside EDF6.exe. To roll back, close the game and copy the saved DLL to Mods/Plugins.

## Third-party notices

The loader API is from EDFModLoader. Steam discovery in the installers is adapted
from EDF6Coop. Their MIT notices are under third_party and included in release ZIPs.
