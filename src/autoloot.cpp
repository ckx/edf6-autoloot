#include "autoloot.h"

#include <windows.h>

#include <cstddef>
#include <cstring>

#include "guard.h"
#include "log.h"

namespace autoloot {
namespace {

constexpr std::uint32_t kExitDispatch = 0x1DED10;     // internal_Exit's std::function: jmp [vtable+0x18]
constexpr std::uint32_t kIsOnlineMode = 0x1B5DF0;     // the mission script's bool IsOnlineMode()
constexpr std::uint32_t kPickup = 0x2C7540;           // DropItemManager pickup(manager, player, type, amount)
constexpr std::uint32_t kItemUpdate = 0x2CA350;       // DropItemManager update: removes items marked picked up
constexpr std::uint32_t kDropItemManager = 0x20B2988;  // DropItemManager*

// DropItemManager
constexpr std::size_t kItems = 0xDE0;  // std::list of items: its sentinel node
constexpr std::size_t kWeaponCount = 0xE08;
constexpr std::size_t kArmorCount = 0xE0C;
// An item list node holds its unit at +0x18; the unit its type at +0xC0 and, at +0xC4, a byte set once it has been
// picked up or has fallen out of the map.
constexpr std::size_t kNodeUnit = 0x18;
constexpr std::size_t kUnitType = 0xC0;
constexpr std::size_t kUnitTaken = 0xC4;
enum ItemType : std::uint32_t { kWeapon = 0, kArmor = 1 };
// Far above the game's 128 items on the field: a list this long is not the game's item list.
constexpr std::size_t kMaxItems = 1 << 16;

using IsOnlineModeFn = bool (*)();
using PickupFn = void (*)(std::uintptr_t manager, void* player, std::uint32_t type, float amount);
using ExitFn = void (*)(void* script, int result);

// Game code AutoLoot calls or relies on, by FNV-1a 64 of its bytes. None of these ranges holds a relocation.
struct Code {
    const char* name;
    std::uint32_t rva;
    std::uint32_t size;
    std::uint64_t hash;
};
constexpr Code kCode[] = {
    {"MissionScriptASImplement::Exit", kExit, 0x8F, 0xB0D69B1723C74D9A},
    {"internal_Exit dispatch", kExitDispatch, 0xD, 0xE1A44BC981EDE01D},
    {"IsOnlineMode", kIsOnlineMode, 0x2B, 0xA71A18502F48BC92},
    {"DropItemManager pickup", kPickup, 0x190, 0x4CEF0202A8725658},
    {"DropItemManager update", kItemUpdate, 0x1AF, 0xAFBFAFE9F194311A},
};

std::uint64_t Fnv1a(const unsigned char* bytes, std::size_t size) {
    std::uint64_t hash = 0xCBF29CE484222325;
    for (std::size_t i = 0; i < size; ++i) hash = (hash ^ bytes[i]) * 0x100000001B3;
    return hash;
}

// Reads the headers of the module the loader found as EDF.dll, which are mapped and readable.
bool SupportedImage(const unsigned char* image) {
    IMAGE_DOS_HEADER dos{};
    std::memcpy(&dos, image, sizeof(dos));
    if (dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew <= 0 || dos.e_lfanew > 0x1000) return false;
    IMAGE_NT_HEADERS64 nt{};
    std::memcpy(&nt, image + dos.e_lfanew, sizeof(nt));
    return nt.Signature == IMAGE_NT_SIGNATURE && nt.FileHeader.Machine == IMAGE_FILE_MACHINE_AMD64 &&
           nt.FileHeader.TimeDateStamp == kImageTimeDateStamp && nt.OptionalHeader.SizeOfImage == kImageSize;
}

unsigned char* game = nullptr;
ExitFn gameExit = nullptr;

void LogCollection(const Collection& collection) {
    switch (collection.outcome) {
    case Outcome::Online:
        Log("Mission cleared online: crates left to the players (online collection is not enabled)");
        return;
    case Outcome::NoManager:
        Log("Mission cleared, but there is no item manager: nothing collected");
        return;
    case Outcome::Collected:
    case Outcome::Fault:
        Log("Mission cleared: took %u weapon and %u armor crates; the game counted %u weapon and %u armor%s",
            collection.weapons, collection.armor, collection.weaponCredit, collection.armorCredit,
            collection.outcome == Outcome::Fault ? ". STOPPED early (fault or unexpected item list): the rest stay" : "");
        return;
    }
}

// Slot 3 of MissionScriptASImplement's vtable.
void MissionExit(void* script, int result) {
    if (result == kMissionResultClear) LogCollection(CollectRemaining(game));
    gameExit(script, result);
}

}  // namespace

Collection CollectRemaining(unsigned char* base) {
    Collection collection;
    std::uintptr_t manager = 0;
    if (!ReadGuarded(base + kDropItemManager, manager)) return collection;
    if (!manager) {
        collection.outcome = Outcome::NoManager;
        return collection;
    }
    const auto isOnlineMode = reinterpret_cast<IsOnlineModeFn>(base + kIsOnlineMode);
    bool online = true;
    if (!Guarded([&] { online = isOnlineMode(); })) return collection;
    if (online) {
        collection.outcome = Outcome::Online;
        return collection;
    }

    const auto pickup = reinterpret_cast<PickupFn>(base + kPickup);
    std::uint32_t weaponsBefore = 0, armorBefore = 0, weaponsAfter = 0, armorAfter = 0;
    if (!ReadGuarded(manager + kWeaponCount, weaponsBefore) || !ReadGuarded(manager + kArmorCount, armorBefore))
        return collection;
    // In memory, not registers: after a fault they still say how many crates were taken.
    volatile unsigned weapons = 0, armor = 0;
    bool ended = false;
    const bool ran = Guarded([&] {
        const auto pointer = [](std::uintptr_t at) { return *reinterpret_cast<const std::uintptr_t*>(at); };
        const auto count = [](std::uintptr_t at) { return *reinterpret_cast<const std::uint32_t*>(at); };
        // The pickup only counts (and with a player, plays a sound and heals): the list stays as it is meanwhile.
        const std::uintptr_t sentinel = pointer(manager + kItems);
        std::uintptr_t node = pointer(sentinel);
        for (std::size_t i = 0; node != sentinel && i < kMaxItems; ++i) {
            const std::uintptr_t unit = pointer(node + kNodeUnit);
            const std::uint32_t type = count(unit + kUnitType);
            auto* taken = reinterpret_cast<volatile std::uint8_t*>(unit + kUnitTaken);
            if (!*taken && (type == kWeapon || type == kArmor)) {
                // Marked before the credit: a fault inside the pickup leaves the crate taken, never counted twice.
                *taken = 1;
                // No player: the pickup's sound is for one player walking over one crate.
                pickup(manager, nullptr, type, 0.0f);
                if (type == kWeapon) weapons = weapons + 1;
                else armor = armor + 1;
            }
            node = pointer(node);
        }
        ended = node == sentinel;
    });
    collection.outcome = ran && ended ? Outcome::Collected : Outcome::Fault;
    collection.weapons = weapons;
    collection.armor = armor;
    if (ReadGuarded(manager + kWeaponCount, weaponsAfter) && ReadGuarded(manager + kArmorCount, armorAfter)) {
        collection.weaponCredit = weaponsAfter - weaponsBefore;
        collection.armorCredit = armorAfter - armorBefore;
    }
    return collection;
}

bool InstallMissionClearHook(unsigned char* base) {
    if (!SupportedImage(base)) {
        Log("REFUSED: EDF.dll is not the supported build (TimeDateStamp %08X, SizeOfImage %X); nothing was changed",
            kImageTimeDateStamp, kImageSize);
        return false;
    }
    for (const Code& code : kCode) {
        if (Fnv1a(base + code.rva, code.size) != code.hash) {
            Log("REFUSED: EDF+%X (%s) is not the expected code; nothing was changed", code.rva, code.name);
            return false;
        }
    }
    auto* slot = reinterpret_cast<std::uintptr_t*>(base + kExitSlot);
    const auto expected = reinterpret_cast<std::uintptr_t>(base + kExit);
    if (*slot != expected) {
        Log("REFUSED: the mission exit slot EDF+%X points to %p, not EDF+%X: another mod has it; nothing was changed",
            kExitSlot, reinterpret_cast<void*>(*slot), kExit);
        return false;
    }
    game = base;
    gameExit = reinterpret_cast<ExitFn>(expected);
    const auto hook = reinterpret_cast<std::uintptr_t>(&MissionExit);
    DWORD protection = 0;
    if (!VirtualProtect(slot, sizeof(*slot), PAGE_READWRITE, &protection)) {
        Log("REFUSED: could not unprotect EDF+%X (error %lu); nothing was changed", kExitSlot, GetLastError());
        return false;
    }
    InterlockedExchangePointer(reinterpret_cast<void* volatile*>(slot), reinterpret_cast<void*>(hook));
    VirtualProtect(slot, sizeof(*slot), protection, &protection);
    Log("Installed: weapon and armor crates left on the field are collected when a mission is cleared offline "
        "(mission exit slot EDF+%X)", kExitSlot);
    return true;
}

}  // namespace autoloot
