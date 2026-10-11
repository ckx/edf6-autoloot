# Working instructions

## Purpose and scope

Build a small EDFModLoader plugin that collects remaining weapon and armor crates
on mission success. The accepted behavior is in `docs/design.md`.

Keep this independent of the sibling `edf-coop-stable` repository. Do not modify
its networking or require it as a runtime dependency. Use it as a reference for
the loader API, game layout, guarded reads, hooks and native-code test fixtures.
Check licenses and preserve required notices before copying source.

Do not add hotkeys, continuous collection, health collection, changed loot rates,
raised pickup limits, replacement equipment, mission-script overrides or unrelated
patches without a new scope decision from the user.

## Implementation

- Use C++20 and 4-space indentation. Keep both MSVC and clang/MinGW-w64 buildable.
- Windows and Proton are equally supported. Exercise Windows APIs under Wine.
- Prefer the game's existing pickup and reward paths over writing reward counters
  or saves directly. Prove their calling conventions and ownership first.
- Run collection on the appropriate game thread while the mission and items live.
  Do not keep raw item pointers across mission transitions or asynchronous work.
- Treat pickup lists as mutable during collection. Bound traversal and validate
  identities and object lifetimes before calling game functions.
- Verify the game build and every original hook byte before patching. Refuse
  unsupported or conflicting code cleanly. Roll back partial hook installation.
- Do not use `__try`/`__except` for portable memory probes. Follow the proven
  guarded-read approach in the sibling project. A recovered fault does not undo
  a partially completed game call and is not permission to retry that pickup.
- Repeated success notifications must not duplicate rewards. Reset mission state
  correctly for retry, teardown and the next attempt.
- Never assume host-only collection or per-client collection is correct. Document
  reward authority and synchronization evidence before enabling online collection.
- Keep hooks and logs small. Add helpers only when they enforce an invariant,
  manage lifetime or remove substantial repeated logic.

## Evidence and tests

Read `docs/research.md` before using its addresses. They are research leads from
another project's supported binary, not verified AutoLoot interfaces.

Record each hook's supported image, RVA, expected bytes, calling convention,
thread, object lifetime and evidence in the research document. Distinguish observed
behavior from hypotheses and retain unresolved questions.

Before adding a test, state the plausible defect, the production path it exercises
and the observable failure. Prefer the smallest fixture that reaches real code.
Do not reproduce the implementation in a mock or test constants and source spelling.

Test the milestones in `docs/implementation-plan.md`. Keep game binaries external,
using `EDF6_GAME_DIR`. Tests requiring an absent game binary should report skipped;
validation claimed against game code must require that binary and report skips.
Automated test success is not evidence of live multiplayer compatibility.
Run stress tests only for a relevant defect or explicitly requested validation.

## Repository and delivery

Keep documentation aligned with implemented behavior and mark planned work clearly.
Do not add placeholder build commands or claim unperformed tests passed.
Keep game data, archives, binaries, saves, logs and temporary output out of Git.
Use `local/` for persistent machine-specific evidence and `tmp/` for disposable work.
Do not redistribute game scripts or data from the old Lootmaster archive.

Inspect Git status before edits. Preserve unrelated work. Never discard commits or
uncommitted files to make an operation convenient. When an origin exists, fetch and
integrate upstream work before landing changes. Do not invent a remote destination.

Make commits for meaningful completed work. Use a lowercase imperative subject,
50 to 70 characters, with no prefix or trailing period. Add a short body only when
needed. Do not add AI attribution, co-author trailers or session links.

When replacing installed Lootmaster files, use the saved installation manifest and
check current hashes. Move matching files to a recoverable backup. Preserve any
later edits and all other mods. Close the game before changing its installation,
back up saves before live testing, and preserve the existing loader and Proton
launch settings. Installing experimental builds is a separate step from building.
