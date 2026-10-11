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

The CMake project also sets MSVC options, but no MSVC build has been run yet.

## Installation

Not yet released. For a test install, close the game, back up saves, and move the
Lootmaster files aside first: its mission scripts and patches change the same
results. Then copy `EDF6AutoLoot.dll` into `Mods/Plugins`, beside EDF6Coop. The
existing loader is used. On Proton it needs this launch option:

```text
WINEDLLOVERRIDES="winmm=n,b" %command%
```

Removing the DLL disables AutoLoot.
