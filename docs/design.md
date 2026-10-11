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

## Proposed runtime

The hook locations and call signatures remain research work.

1. Recognize an authoritative successful mission transition while the item manager
   is alive and reward credit and synchronization can still complete.
2. Start collection at most once for that mission attempt on the game thread.
3. Find eligible weapon and armor crates and invoke the normal collection path,
   respecting item ownership, removal and reward limits.
4. Allow normal mission teardown, result calculation and saving to proceed.
5. Clear attempt state on the appropriate lifecycle transitions.

The collection strategy must tolerate an item disappearing or list nodes changing
as pickups execute. Decide whether to iterate, capture stable identities or use an
existing bulk operation after tracing the game's behavior. A pointer snapshot alone
is not a lifetime guarantee.

Success may be announced before the final crate can spawn. Research must establish
the collection cutoff, synchronization ordering and whether a bounded drain within
the successful transition is needed. Do not assume an arbitrary delay is sufficient.

An error after partial collection must not cause already credited items to be
awarded again. Keep normal game progression working if collection cannot complete,
and log the failure once with enough context to diagnose it.

## Multiplayer

Determine which machine owns pickup removal and reward credit, how peers learn
about pickups, and when the mission freezes results. Calling the same function on
every client or calling it only on the host is unproven.

Validate host and client rewards, downed players, simultaneous ordinary pickups,
host changes, disconnects and the completion boundary under packet loss.
Test alongside EDF6Coop and with AutoLoot loaded in either plugin order.

Host-only installation is desirable but is not a promise. Record the proven
installation requirement after testing. Keep online collection disabled in the
prototype until authority and reward behavior are understood. A custom network
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
