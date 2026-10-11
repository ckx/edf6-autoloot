# Implementation plan

Repository setup is complete. All implementation milestones below are pending.

## 1. Establish the native interfaces

Trace the mission-success, pickup, teardown and result paths in the supported
EDF.dll. Compare the relevant original scripts with the inspected package only as
needed. Record findings in `research.md`.

Done when the success hook and complete pickup interface have documented signatures,
expected bytes, thread and lifetime requirements, and online authority questions are
explicit. Do not begin by guessing the signature of the pickup candidate.

## 2. Build an offline prototype

Add CMake, the loader entry point, bounded logging, image checks and the smallest
hook implementation needed. Add MSVC and clang/MinGW-w64 build scripts and Wine test
support. Keep game binaries external through `EDF6_GAME_DIR`.

Exercise actual native pickup code in an existing-style fixture where practical.
Then collect real weapon and armor crates at mission success in an offline session.
Use a backed-up save and remove the original Lootmaster overrides from the test
installation before attributing any behavior to this plugin.

Done when reward totals match normal collection for eligible remaining crates,
health remains untouched, and removal of the DLL restores ordinary behavior.

## 3. Verify attempt state and refusal

| Scenario | Expected result |
| --- | --- |
| Success with weapons, armor and health | Eligible loot credited once; health untouched |
| Success with no eligible crates | No reward change or fault |
| Repeated success notification | No duplicate credit |
| Manual pickup overlaps collection | Each crate credited at most once |
| Failure or retreat | No automatic collection; normal reward handling retained |
| Retry, then success | No stale item or reward state from the previous attempt |
| Consecutive missions | Fresh collection state for each attempt |
| Reward limit reached | Game's ordinary limit behavior |
| Item list changes during pickup | Safe progress without stale dereferences |
| Partial collection failure | Normal mission exit; no duplicate retry of credited items |
| Unsupported image or mismatched hook | Clean refusal without partial patching |
| Refusal followed by plugin unload | No surviving callbacks into the plugin |

Use tests that reach the production path and check observable outcomes. A synthetic
state test cannot establish that an actual mission lifecycle hook runs at the right
time; retain live checks for that boundary.

## 4. Establish online behavior

Prove removal authority, per-player reward credit and result synchronization before
enabling online collection. Start with two processes, then verify a full room.
Exercise host/client roles, downed players, disconnects, host change where supported,
late drops, loss/reordering at completion and both plugin load orders with EDF6Coop.

Done when every player receives the expected rewards once in real play, the required
installation arrangement is documented, and failures do not block mission completion.
Automated GameNet-style tests supplement live sessions; they do not replace them.

## 5. Package and migrate

Build and verify on Windows and Linux/Wine, then perform live Proton checks.
Package the plugin and concise install/removal instructions. Document any proven
limits and required peer installation. Avoid bundling an older loader over a working
one. Include required third-party notices for any reused code.

For the current installation, close the game, back up saves and move the old
Lootmaster files to a recoverable backup using the local manifest and current hashes.
Preserve later edits, the existing loader and EDF6Coop. Install the new DLL only as
an explicit test or delivery step, and retain a straightforward rollback.

Done when installation, removal and rollback have been exercised and campaign/DLC,
Windows/Proton and multiplayer results are recorded with remaining gaps stated.
