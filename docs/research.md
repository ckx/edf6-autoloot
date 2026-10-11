# Research notes

## Evidence collected on 2026-10-11

These notes describe local source and archive inspection. No AutoLoot hook or live
collection path has been implemented or tested.

### Existing Lootmaster package

Local archive: `/hayate/files/mods/Lootmaster-1-0-8-1729125707.zip`.
Its filename identifies version 1.0.8; its bundled readme still says v0.6.

The archive contains 204 mission-script overrides, a main-script override, default
configuration, weapon tables and text, a replacement Fencer DLC accessory, Patcher,
four patch files and a loader. Its pickup patch raises the map limit from 128 to
12,800 and the mission limit from 1,024 to 102,400.

In the inspected M001 and M005 scripts, a `LOOTMASTER` block calls
`CreatePlayer_InitWeapon(...)` and waits one second before later reaching
`MissionClear()`. The readme says the Gunner's Exoskeleton accessory was repurposed
for the looter function. These observations explain parts of the package's approach;
they do not establish its complete collection mechanism or every file's changes.

The supplied main script runs `Mission()`, releases mission-scene resources, then
calls `Match_End(result)` and performs result synchronization. `Result_Common`
later calls `ResolveResult()` and `ApplyResult(...)`. This suggests results-screen
collection may be too late. Compare against the original game and trace native
lifetime and synchronization before choosing a hook.

### Existing EDF6Coop source

Local checkout: `/hayate/files/mods/edf-coop-stable`.

The following leads come from `game/radar.cpp` and its comments. Addresses are RVAs
relative to EDF.dll, not absolute process addresses.

| Lead | Value | Evidence and limit |
| --- | --- | --- |
| DropItemManager global pointer | `0x20B2988` | Read by the radar implementation |
| Manager constructor | `0x2C68D0` | Named in its comments |
| Item list sentinel field | manager + `0xDE0` | Walked by the radar implementation |
| Item unit field | list node + `0x18` | Read by the radar implementation |
| Item type field | unit + `0xC0` | Read by the radar implementation |
| Item types | 0 weapon, 1 armor, 2 small heal, 3 large heal | Mapped by the radar implementation |
| Pickup function candidate | `0x2C7540` | Comment says it credits weapons/armor or heals |
| Unit initialization lead | `0x2C83D1` | Comment identifies the item-type assignment |

The pickup signature, preconditions, authority, item removal, reward recipient and
call timing are not established by the radar code. Its read-only list walk must not
be copied as a collection loop without accounting for mutation.

Useful reference files in that checkout:

- `game/plugin.cpp`: loader entry point, image checks and hook refusal.
- `game/code.*` and `game/midhook.*`: patch and thunk support.
- `game/guard.h`: guarded memory access that works across the two toolchains.
- `tests/game/native_runtime.h`: privately mapped game-code test support.
- `tests/gamenet/`: multi-process game-code fixtures and explicit ABI layouts.
- `cmake/mingw-w64.cmake`: cross-compilation toolchain.
- `third_party/EDFModLoader/PluginAPI.h`: plugin API reference.

Check the license applicable to each source before reuse. Record provenance and
retain required notices. The new repository has no chosen distribution license yet.

## Research deliverables

For every candidate interface, record the image hash and identity, RVA, relevant
instructions, expected bytes, signature, callers, thread and lifetime. Separate
observed facts, inference and open questions.

Resolve these questions before selecting the implementation:

- Where does shared mission success commit, including campaign and DLC missions?
- When are items destroyed and rewards finalized? Can drops arrive after success?
- Which native path performs a complete pickup rather than only crediting a counter?
- What identifies an item across list mutation, and when can it be freed?
- How do normal pickups reach each online player's results?
- Can the transition support collection without a timer or blocking network wait?
- What happens when a player is downed or a host disconnects during completion?

## Local installation and migration evidence

Game directory:
`/mnt/kosmos/SteamLibrary/steamapps/common/EARTH DEFENSE FORCE 6`.

The earlier installation added 218 archive files and preserved the existing
`winmm.dll`, `Mods/Plugins/EDF6Coop.dll` and `Mods/Plugins/EDF6Coop.ini`.
Its manifest records installed relative paths and SHA-256 hashes, plus protected
file hashes. Repository setup preserves a copy at
`local/lootmaster-install-manifest.json` if the original temporary record is available.
That copy is intentionally ignored by Git and must be retained locally for migration.

Before replacement, compare every installed file with the manifest. Back up matching
files by moving them aside outside active mod paths; retain changed files for review.
Do not delete whole Mods subdirectories or remove plugins based only on their name.
The manifest describes one installation and is not a generic uninstaller.
