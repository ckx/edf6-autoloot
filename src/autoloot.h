#pragma once
#include <cstdint>

namespace autoloot {

// The EDF.dll build every address below was taken from (Steam, 2025-01 update). docs/research.md records the
// evidence for each one.
constexpr std::uint32_t kImageTimeDateStamp = 0x678CCB46;
constexpr std::uint32_t kImageSize = 0x22CE000;

// The mission script ends a mission with internal_Exit(result). Its std::function (1DED10) calls slot 3 of
// MissionScriptASImplement's vtable, Exit (1DDE90), which stores the result for the main script and, for any result
// but a clear, empties DropItemManager. Every clear (MissionClear, FinalMissionClear, MissionEscapeClear and the
// rest in Mission/AsCommon.h) calls internal_Exit(MISSION_RESULT_CLEAR) after its banner and fade.
constexpr std::uint32_t kExitSlot = 0x1793088;
constexpr std::uint32_t kExit = 0x1DDE90;
constexpr int kMissionResultClear = 1;

enum class Outcome {
    Collected,  // every crate on the list was looked at
    Online,     // an online session: nothing was touched
    NoManager,  // no DropItemManager
    Fault,      // a read or the pickup faulted, or the list did not end: the crates taken so far were taken once
};

struct Collection {
    Outcome outcome = Outcome::Fault;
    unsigned weapons = 0;  // weapon crates taken
    unsigned armor = 0;    // armor crates taken
    // How much the game's own counts of this attempt's pickups rose. It counts at most 1024 pickups per attempt,
    // so these can be lower than the crates taken, as when a player walks over the same crates.
    unsigned weaponCredit = 0;
    unsigned armorCredit = 0;
};

// Takes every weapon and armor crate still on the field as a player walking over it would: the game's pickup
// (2C7540) credits it and the crate is marked picked up, so the manager's next update removes it. Health stays.
// Crates already picked up are skipped, so a second call takes nothing. Offline only. Call it on the thread that
// runs the mission script, while the mission still lives.
Collection CollectRemaining(unsigned char* game);

// The supported EDF.dll, every byte of game code AutoLoot calls or relies on, and the Exit slot still holding the
// game's Exit: then the slot is pointed at the plugin, which collects on a clear and then calls the game's Exit.
// False, logged and with nothing changed otherwise.
bool InstallMissionClearHook(unsigned char* game);

}  // namespace autoloot
