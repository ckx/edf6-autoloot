// AutoLoot's collection and hook installation against the real EDF.dll, mapped without running its initialisation.
// The game's own pickup (2C7540) credits the crates, its IsOnlineMode (1B5DF0) decides online play, and its lookup of
// an item by id (2C7B10), which skips items already picked up, shows which crates the game now takes as gone.
//   CollectTests EDF.dll
// Exits 77 (skipped) without EDF.dll.
#include <windows.h>

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <filesystem>

#include "autoloot.h"

using namespace autoloot;

namespace {

int failures = 0;

void Check(bool condition, const char* what) {
    if (!condition) {
        ++failures;
        std::printf("FAIL: %s\n", what);
    }
}

unsigned char* game = nullptr;

// Game globals and the item lookup, from the supported image.
constexpr std::uint32_t kSessionModes = 0x20B2890;  // read by IsOnlineMode and by the pickup
constexpr std::uint32_t kDropItemManager = 0x20B2988;
constexpr std::uint32_t kTakeItemById = 0x2C7B10;  // bool (manager, id): marks the first item with `id` not yet taken
constexpr std::uint32_t kPickup = 0x2C7540;

template <typename T>
void Poke(void* at, T value) {
    DWORD protection = 0;
    VirtualProtect(at, sizeof(T), PAGE_EXECUTE_READWRITE, &protection);
    std::memcpy(at, &value, sizeof(T));
    VirtualProtect(at, sizeof(T), protection, &protection);
    FlushInstructionCache(GetCurrentProcess(), at, sizeof(T));
}

template <typename T>
T Peek(const void* at) {
    T value{};
    std::memcpy(&value, at, sizeof(T));
    return value;
}

enum Type : std::uint32_t { kWeapon = 0, kArmor = 1, kHealSmall = 2, kHealBig = 3 };

// A DropItemManager as the game lays it out: a std::list of items at +0xDE0 (nodes hold the unit at +0x18), the
// pickups of this attempt at +0xE04 (all), +0xE08 (weapons) and +0xE0C (armor). Units: type +0xC0, taken +0xC4,
// id +0xC8. The session modes as IsOnlineMode reads them: current index at +0x38 (-1: none), mode array at +0x20,
// each mode's online flag at +0x68 into its virtual base.
#pragma warning(push)
#pragma warning(disable : 4324)  // Intentional padding for the game buffers aligned to 16 bytes.
struct Field {
    struct Node {
        Node* next;
        Node* prev;
        std::uint64_t unused;
        unsigned char* unit;
    };
    alignas(16) unsigned char manager[0x1000]{};
    Node sentinel{&sentinel, &sentinel, 0, nullptr};
    std::deque<Node> nodes;
    std::deque<std::array<unsigned char, 0x100>> units;
    alignas(16) unsigned char modes[0x40]{};
    void* modeList[1]{};
    alignas(16) unsigned char mode[0x20]{};
    alignas(16) unsigned char modeBase[0x100]{};

    explicit Field(bool online = false) {
        Poke(manager + 0xDE0, &sentinel);
        Poke<std::int32_t>(modes + 0x38, online ? 0 : -1);
        Poke<void*>(modes + 0x20, modeList);
        modeList[0] = mode;
        Poke<void*>(mode + 0x10, modeBase);
        Poke<std::int32_t>(modeBase + 0x8, 0x10);
        Poke<std::int32_t>(modeBase + 0x10 + 0x68, online ? 1 : 0);
        Poke<void*>(game + kSessionModes, modes);
        Poke<void*>(game + kDropItemManager, manager);
    }
    ~Field() {
        Poke<void*>(game + kSessionModes, nullptr);
        Poke<void*>(game + kDropItemManager, nullptr);
    }
    // Appends an item; returns its id.
    std::uint32_t Add(Type type, bool taken = false) {
        const auto id = static_cast<std::uint32_t>(units.size() + 1);
        auto& unit = units.emplace_back();
        unit.fill(0);
        Poke(unit.data() + 0xC0, static_cast<std::uint32_t>(type));
        Poke<std::uint8_t>(unit.data() + 0xC4, taken ? 1 : 0);
        Poke(unit.data() + 0xC8, id);
        Node& node = nodes.emplace_back(Node{&sentinel, sentinel.prev, 0, unit.data()});
        sentinel.prev->next = &node;
        sentinel.prev = &node;
        Poke(manager + 0xDE8, static_cast<std::uint64_t>(nodes.size()));
        return id;
    }
    std::uint32_t Count(std::size_t offset) const { return Peek<std::uint32_t>(manager + offset); }
    std::uint32_t Pickups() const { return Count(0xE04); }
    std::uint32_t Weapons() const { return Count(0xE08); }
    std::uint32_t Armor() const { return Count(0xE0C); }
    // What the game's own lookup says: true when the item was still there to take (it is taken now).
    bool StillThere(std::uint32_t id) {
        using TakeById = bool (*)(void* manager, std::uint32_t id);
        return reinterpret_cast<TakeById>(game + kTakeItemById)(manager, id);
    }
};

#pragma warning(pop)

// Defect: crates not credited, health taken, or crates already picked up credited again.
void CollectsWeaponsAndArmorOnly() {
    Field field;
    const auto weapon = field.Add(kWeapon);
    const auto armor = field.Add(kArmor);
    const auto small = field.Add(kHealSmall);
    const auto big = field.Add(kHealBig);
    const auto picked = field.Add(kWeapon, true);
    const auto armor2 = field.Add(kArmor);
    const Collection c = CollectRemaining(game);
    Check(c.outcome == Outcome::Collected, "offline collection completes");
    Check(c.weapons == 1 && c.armor == 2, "takes the one weapon and two armor crates left");
    Check(c.weaponCredit == 1 && c.armorCredit == 2, "reports what the game counted");
    Check(field.Weapons() == 1 && field.Armor() == 2 && field.Pickups() == 3, "the game's pickup counted them");
    Check(!field.StillThere(weapon) && !field.StillThere(armor) && !field.StillThere(armor2),
          "the game takes the collected crates as picked up");
    Check(!field.StillThere(picked), "a crate picked up before stays picked up");
    Check(field.StillThere(small) && field.StillThere(big), "health stays on the field");
}

// Defect: a repeated clear notification credits the same crates again.
void SecondClearTakesNothing() {
    Field field;
    field.Add(kWeapon);
    field.Add(kArmor);
    CollectRemaining(game);
    const Collection again = CollectRemaining(game);
    Check(again.outcome == Outcome::Collected && again.weapons == 0 && again.armor == 0, "second call takes nothing");
    Check(field.Weapons() == 1 && field.Armor() == 1, "second call counts nothing");
}

// Defect: collection bypasses the game's limit of 1024 pickups per attempt.
void KeepsTheGamesPickupLimit() {
    Field field;
    Poke<std::uint32_t>(field.manager + 0xE04, 1023);
    const auto first = field.Add(kWeapon);
    const auto second = field.Add(kWeapon);
    const Collection c = CollectRemaining(game);
    Check(c.weapons == 2 && c.weaponCredit == 1, "at the limit the crate is taken, as walking over it would, uncounted");
    Check(field.Pickups() == 1024 && field.Weapons() == 1, "the game stops counting at 1024");
    Check(!field.StillThere(first) && !field.StillThere(second), "both crates are gone");
}

// Defect: collection runs in an online session, where reward authority is unproven.
void LeavesOnlineSessionsAlone() {
    Field field(true);
    const auto weapon = field.Add(kWeapon);
    const Collection c = CollectRemaining(game);
    Check(c.outcome == Outcome::Online && c.weapons == 0, "online: nothing collected");
    Check(field.Weapons() == 0 && field.Pickups() == 0, "online: nothing counted");
    Check(field.StillThere(weapon), "online: the crate stays");
}

// Defect: a clear without an item manager, or with an empty field, crashes or counts.
void EmptyCases() {
    Check(CollectRemaining(game).outcome == Outcome::NoManager, "no manager: nothing to do");
    Field field;
    const Collection c = CollectRemaining(game);
    Check(c.outcome == Outcome::Collected && c.weapons == 0 && c.armor == 0 && field.Pickups() == 0,
          "empty field: nothing taken");
}

// Defect: an unreadable item crashes the game, or the crates before it are credited again later.
void FaultStopsWithoutRecounting() {
    Field field;
    const auto weapon = field.Add(kWeapon);
    const auto armor = field.Add(kArmor);
    unsigned char* const unit = field.nodes.back().unit;
    field.nodes.back().unit = reinterpret_cast<unsigned char*>(0x10);
    const Collection c = CollectRemaining(game);
    Check(c.outcome == Outcome::Fault, "an unreadable unit is reported as a fault");
    Check(c.weapons == 1 && c.weaponCredit == 1 && field.Weapons() == 1, "the crate before it was taken once");
    field.nodes.back().unit = unit;
    Check(!field.StillThere(weapon), "and stays taken");
    Check(CollectRemaining(game).weapons == 0 && field.Weapons() == 1, "a later call does not count it again");
    Check(!field.StillThere(armor), "and takes what the fault left");
}

std::uintptr_t ExitSlot() { return Peek<std::uintptr_t>(game + kExitSlot); }

// Defect: the hook patches a build or code it does not know, or replaces another mod's hook.
void InstallRefusesUnexpectedCode() {
    const auto original = reinterpret_cast<std::uintptr_t>(game + kExit);
    Check(ExitSlot() == original, "the slot starts at the game's Exit");

    const auto byte = Peek<std::uint8_t>(game + kPickup + 0x100);
    Poke<std::uint8_t>(game + kPickup + 0x100, byte ^ 0xFF);
    Check(!InstallMissionClearHook(game) && ExitSlot() == original, "refuses a changed pickup");
    Poke<std::uint8_t>(game + kPickup + 0x100, byte);

    Poke<std::uintptr_t>(game + kExitSlot, original + 1);
    Check(!InstallMissionClearHook(game) && ExitSlot() == original + 1, "refuses a slot another mod took");
    Poke<std::uintptr_t>(game + kExitSlot, original);

    const auto nt = Peek<std::uint32_t>(game + 0x3C);  // IMAGE_DOS_HEADER::e_lfanew
    unsigned char* const header = game + nt + 8;      // IMAGE_FILE_HEADER::TimeDateStamp
    const auto time = Peek<std::uint32_t>(header);
    Poke<std::uint32_t>(header, time + 1);
    Check(!InstallMissionClearHook(game) && ExitSlot() == original, "refuses another build");
    Poke<std::uint32_t>(header, time);

    Check(InstallMissionClearHook(game), "installs on the supported build");
    Check(ExitSlot() != original, "the slot now leads to the plugin");
    Check(!InstallMissionClearHook(game), "a second install refuses: the slot is taken");
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2 || !std::filesystem::exists(argv[1])) {
        std::printf("SKIPPED: no EDF.dll (set EDF6_GAME_DIR)\n");
        return 77;
    }
    game = reinterpret_cast<unsigned char*>(LoadLibraryExA(argv[1], nullptr, DONT_RESOLVE_DLL_REFERENCES));
    if (!game) {
        std::printf("FAIL: could not map %s (error %lu)\n", argv[1], GetLastError());
        return 1;
    }
    CollectsWeaponsAndArmorOnly();
    SecondClearTakesNothing();
    KeepsTheGamesPickupLimit();
    LeavesOnlineSessionsAlone();
    EmptyCases();
    FaultStopsWithoutRecounting();
    InstallRefusesUnexpectedCode();
    std::printf(failures ? "%d check(s) failed\n" : "all checks passed\n", failures);
    return failures ? 1 : 0;
}
