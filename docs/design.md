# Design

## Accepted behavior

Collect remaining weapon and armor crates when a mission succeeds. The user chose
this trigger on 2026-10-11.

Only crates still present in the mission are eligible. Use normal game reward
rules, including limits, random rolls and distribution. Crates already despawned
are not recreated. Health stays untouched. Failure, retreat and retry do not
trigger collection; the game retains its ordinary handling of previously earned
rewards in those cases.

The initial deliverable is one `EDF6AutoLoot.dll` for the existing EDFModLoader,
with a short diagnostic log. Installation enables it and removing the DLL disables
it. No user interface or settings file is needed for the first version.

## Repository boundary

Keep AutoLoot in a sibling repository so its development and removal are independent
of EDF6Coop's networking fixes. Runtime cooperation should use the game's ordinary
interfaces. Copy only the small pieces of infrastructure actually needed, after
checking their licenses. Do not introduce a shared framework or a dependency on the
sibling checkout to build this project.

## Runtime

Implemented for offline play. `docs/research.md` holds the evidence for each step.

1. Every mission clear ends in the script call `internal_Exit(MISSION_RESULT_CLEAR)`,
   after the clear banner and fade. That call reaches slot 3 of
   `MissionScriptASImplement`'s vtable, the game's `Exit`. The plugin points that
   slot at its own function. No game instruction changes.
2. On a clear, before calling the game's `Exit`, the plugin walks DropItemManager's
   item list once. For each weapon or armor crate not yet picked up, it marks the
   crate picked up and calls the game's pickup function. The game counts it under
   its usual 1024-pickup limit and removes the crate on its next update, as when a
   player walks over it. Players do not move, loadouts do not change, and no pickup
   sound or effect plays.
3. The game's `Exit` then runs unchanged, and results, weapon rolls and saving
   follow their normal path.

There is no attempt state. The picked-up flag lives on each crate, so a repeated
clear finds nothing left to take, and a retry or failure empties the list as before.
The pickup does not change the list, so the walk sees a stable list. The walk has a
fixed bound and runs under the guarded-read approach. A fault stops it. Crates
already taken stay taken and are never credited twice. The mission then continues,
and the log records the fault once.

Crates that spawn after the `Exit` call are not collected. The clear path waits
about 10 seconds before that call, and players can still pick up crates meanwhile.

The log `EDF6AutoLoot.log` beside the DLL records installation or refusal, and one
line per clear with the crates taken and the counts the game added.

## Multiplayer

Determine which machine owns pickup removal and reward credit, how peers learn
about pickups, and when the mission freezes results. Calling the same function on
every client or calling it only on the host is unproven.

Validate host and client rewards, downed players, simultaneous ordinary pickups,
host changes, disconnects and the completion boundary under packet loss.
Test alongside EDF6Coop and with AutoLoot loaded in either plugin order.

Host-only installation is desirable but is not a promise. Record the proven
installation requirement after testing. Keep online collection disabled in the
prototype until authority and reward behavior are understood. The prototype asks
the game's own `IsOnlineMode` and leaves every crate alone online. A custom network
protocol would require a new design decision, not an incidental implementation detail.

## Compatibility

Support Windows and Steam Proton. Initially target the same EDF.dll build used by
EDF6Coop, PE TimeDateStamp `0x678CCB46`, after independently verifying image identity
and hook bytes. A timestamp alone does not validate a patch site.

Refuse mismatched bytes and unsupported images without partial hooks. Ensure refusal
and unloading leave no callback pointing into an unloaded plugin. Preserve existing
loader files, Steam settings and unrelated mods during installation.

## Out of scope

Hotkeys, continuous collection, healing, changed drop rates, raised reward or item
limits, weapon substitutions, save editing, per-mission replacements, FOV changes,
chat patches and EOS launcher changes.
