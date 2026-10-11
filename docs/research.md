# Research notes

## AutoLoot interfaces

Static analysis of the supported EDF.dll on 2026-10-11, plus `Mission/AsCommon.h`
read from `Root.cpk`. The offline prototype uses these interfaces. Automated tests
run the pickup, online test and item lookup from the real EDF.dll under Wine. No live
mission has been played with the plugin yet.

### Supported image

`EDF.dll`, PE TimeDateStamp `0x678CCB46`, SizeOfImage `0x22CE000`, SHA-256
`0d5092393a1b9d8f1b75b0c93d8a8f65615a36a92c46e40349c2b66c733d0916`.
Image base `0x180000000`; all addresses below are RVAs.

The plugin checks the timestamp and size, then the FNV-1a 64 hash of each code range
below. None of these ranges contains a base relocation, so the mapped bytes equal the
file bytes. Any mismatch refuses installation before anything is written.

| Range | RVA | Size | FNV-1a 64 |
| --- | --- | --- | --- |
| `MissionScriptASImplement::Exit` | `0x1DDE90` | `0x8F` | `0xB0D69B1723C74D9A` |
| `internal_Exit` dispatch | `0x1DED10` | `0xD` | `0xE1A44BC981EDE01D` |
| `IsOnlineMode` | `0x1B5DF0` | `0x2B` | `0xA71A18502F48BC92` |
| DropItemManager pickup | `0x2C7540` | `0x190` | `0x4CEF0202A8725658` |
| DropItemManager update | `0x2CA350` | `0x1AF` | `0xAFBFAFE9F194311A` |

To recompute a hash, apply FNV-1a 64 (offset basis `0xCBF29CE484222325`, prime
`0x100000001B3`) to the file bytes at the section offset for each RVA.

### Success hook: the mission exit slot

Observed:

- `AsCommon.h` ends every clear (`MissionClear`, `FinalMissionClear`,
  `MissionEscapeClear`, `MissionClear_Kaihen` and the EDF5 variants) through
  `MissionClear_Common` or `MissionClear_Common2`. These show the banner, wait,
  fade out, and then call `internal_Exit(MISSION_RESULT_CLEAR)`. Failure calls
  `internal_Exit` with `MISSION_RESULT_RETRY` or `MISSION_RESULT_ABORT`.
- The registration at `0x1E64F6` binds `void internal_Exit(::MissionResult )` to
  `0x1BCFA0`, which calls the `std::function` at `this+0x48`. That function's type
  is `_Func_impl_no_alloc<lambda_1 in MissionScriptASImplement ctor, void,
  MissionResult>`, vtable `0x17931C8`. Its `_Do_call` at `0x1DED10` is
  `mov rcx,[rcx+8]; mov edx,[rdx]; mov rax,[rcx]; jmp [rax+0x18]`.
- `MissionScriptASImplement` vtable at `0x1793070`. Slot 3, at `0x1793088`, holds
  `EDF+0x1DDE90` (with a DIR64 relocation). The slot is the only reference to that
  function in the image.
- `Exit(this, MissionResult result)`, Microsoft x64: `rcx` is the script object
  and `edx` the result. It writes the scene state (`[this+0xE8]+0x2B8 = 3`) and the
  result (`[this+0xE8]+0x2B4`) and stops sounds. For any result other than 1 it
  calls the DropItemManager reset at `0x2C7900`, which empties the item list and
  zeroes this attempt's pickup counts.
- `MISSION_RESULT_CLEAR` is 1, registered at `0x1E361D`.

Hook: the plugin replaces the slot's pointer with its own function. On result 1 it
collects, then calls the original `Exit` with the same arguments. Other results
pass straight through. No game instruction changes. The slot is written once at
load, after verification. The plugin stays loaded from then on.

Thread (inference): `Exit` runs from the mission script. For a failure, `Exit`
itself empties the DropItemManager item list, so the game treats this context as
one that may change the item list. Collection does the same kind of change in the
same call. A live check of the thread ID has not been made.

### Pickup interface

Observed in the per-player collision loop at `0x2C8AC0` (called from `0x59B938`):

1. For each list node whose unit has `[unit+0xC4] == 0` and is within reach, in an
   online session it first calls `0x2C7D50` (network notice).
2. It calls the pickup `0x2C7540(manager, player, [unit+0xC0], amount)`.
3. It runs an effect through a listener interface, then sets `[unit+0xC4] = 1`.

The pickup, `void (DropItemManager* rcx, Player* rdx, uint32 type r8d, float
amount xmm3)`:

- If `player` is not null and `[player+0x128] & 1` is clear, it plays the sound
  "アイテム取得" (item acquired) through `0x7B2860`. A null player skips it; the
  network receive path at `0x2C8E21` reaches the pickup without the sound in the
  same way.
- Types 0 (weapon) and 1 (armor) touch only the manager. In an online session they
  need `0x784210` (host) to return true. Then, if `[manager+0xE04] < 0x400`, it
  increments `+0xE04` (all pickups) and `+0xE08` (weapons) or `+0xE0C` (armor).
  At the limit nothing is counted.
- Types 2 and 3 heal the player by 15% or 30% and dereference `player`.

AutoLoot calls the pickup with a null player and amount 0, for types 0 and 1 only.
It sets `[unit+0xC4] = 1` before the call rather than after. The pickup does not
read that byte, and a fault inside the pickup then cannot leave the crate to be
credited again.

DropItemManager (`*(EDF+0x20B2988)`):

| Field | Offset | Evidence |
| --- | --- | --- |
| Item list sentinel (`std::list`) | `+0xDE0` | collision loop, update, reset |
| Item count | `+0xDE8` | update decrements it on removal |
| Pickups this attempt (limit 1024) | `+0xE04` | pickup |
| Weapon crates this attempt | `+0xE08` | pickup; failure banking `0x2C8A00`; result `0x2C7F80` |
| Armor crates this attempt | `+0xE0C` | same |
| Node: unit pointer | node `+0x18` | collision loop `0x2C8B20` |
| Unit: type | unit `+0xC0` | collision loop, pickup argument |
| Unit: taken | unit `+0xC4` | set by collision loop, network receive, lookup `0x2C7B10`, fall-out check |
| Unit: id | unit `+0xC8` | network receive and `0x2C7B10` match on it |

Removal: the update at `0x2CA350` (called from `0x7056CB`) first
marks units below y = -1000 as taken, then unlinks every taken node, decrements
`+0xDE8`, destroys the unit and frees the node. A taken crate is therefore removed
on the next update without further credit. Nothing in the pickup changes the list,
so the collection walk sees a stable list.

### Online authority

Observed, not exercised:

- `IsOnlineMode` (`0x1B5DF0`, the script function) reads the session mode table at
  `*(EDF+0x20B2890)`: index `+0x38` (-1 for none), mode array `+0x20`, then the
  online flag at `+0x68` in the mode's virtual base. The pickup applies the same
  test.
- Online, the pickup counts weapons and armor only on the host (`0x784210`).
  `0x2C85C0` sets a client's `+0xE08` and `+0xE0C` from four per-player values at
  `*(EDF+0x20B2890)+0x14FD4..0x14FF0`, which suggests the host's counts reach
  clients through result synchronization.

Collection is off online. The plugin calls the game's `IsOnlineMode` and leaves the
crates alone when it returns true.

### Open questions

- Confirm in live play that `Exit` runs on the thread that updates the item list,
  and that the collected counts reach the results screen.
- Crates that spawn after `internal_Exit(MISSION_RESULT_CLEAR)` are not collected.
  The clear path waits about 10 seconds (BGM stop, banner, fade) before that call,
  and pickups by players stay possible meanwhile.
- Online: whether host-side collection before result synchronization gives every
  player the crates, and what clients do with crates the host marked taken.
- Split-screen play was not examined.

## Evidence collected on 2026-10-11

These notes describe local source and archive inspection done before AutoLoot was
implemented.

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
