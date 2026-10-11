# EDF6 AutoLoot

A planned EDFModLoader plugin for EARTH DEFENSE FORCE 6 that collects remaining
weapon and armor crates when a mission succeeds.

Status: design and repository setup only. There is no plugin or build command yet.

## Behavior

- Collect existing weapon and armor crates on successful mission completion.
- Leave health pickups alone.
- Preserve the game's normal drop rates, reward limits, weapon rolls and reward distribution.
- Do not trigger collection on failure, retreat or retry.
- Support Windows and Linux through Steam Proton.

The intended deliverable is `EDF6AutoLoot.dll`, installed in `Mods/Plugins` beside
EDF6Coop. It uses the existing EDFModLoader. Multiplayer ownership, installation
requirements and compatibility still need verification.

## Documents

- `docs/design.md`: behavior, runtime design and unresolved decisions.
- `docs/research.md`: evidence and reverse-engineering starting points.
- `docs/implementation-plan.md`: milestones and acceptance checks.
- `AGENTS.md`: working rules for contributors and coding agents.

## Development

Use C++20 with CMake. Plan for MSVC on Windows and clang targeting MinGW-w64 on
Linux, with Wine for automated game-code tests and Proton for live play.
The first implementation milestone is to identify the mission-success and pickup
paths in the supported game binary. Build scripts and tests follow that research.

Keep game binaries, extracted assets, saves and local installation records out of
Git. `local/` and `tmp/` are ignored for local evidence and temporary work.

## Installation target

The eventual plugin will use the existing loader and this Steam launch option on
Proton:

```text
WINEDLLOVERRIDES="winmm=n,b" %command%
```

Installation and removal procedures will be added when there is a working build.
The existing Lootmaster package must be moved aside before testing AutoLoot.
