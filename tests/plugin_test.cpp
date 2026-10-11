// Loads the built plugin as EDFModLoader does: LoadLibrary, EML6_Load, and FreeLibrary when it refuses. The plugin
// is copied to <work-folder>\Mods\Plugins, so its log lands there.
//   PluginTests EDF6AutoLoot.dll work-folder            outside the game: refuses, says why, and can be unloaded
//   PluginTests EDF6AutoLoot.dll work-folder EDF.dll    the real EDF.dll mapped: takes the mission exit slot
// Exits 77 (skipped) when EDF.dll is named but missing.
#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#pragma warning(push)
#pragma warning(disable : 4201)
#include "PluginAPI.h"
#pragma warning(pop)

namespace fs = std::filesystem;

namespace {

int failures = 0;

void Check(bool condition, const char* what) {
    if (!condition) {
        ++failures;
        std::printf("FAIL: %s\n", what);
    }
}

std::string ReadText(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

bool InModule(std::uintptr_t address, HMODULE module) {
    const auto base = reinterpret_cast<std::uintptr_t>(module);
    IMAGE_DOS_HEADER dos{};
    std::memcpy(&dos, module, sizeof(dos));
    IMAGE_NT_HEADERS64 nt{};
    std::memcpy(&nt, reinterpret_cast<const unsigned char*>(module) + dos.e_lfanew, sizeof(nt));
    return address >= base && address < base + nt.OptionalHeader.SizeOfImage;
}

using Load = bool(EDFMLAPI*)(PluginInfo*);

}  // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::printf("usage: PluginTests EDF6AutoLoot.dll work-folder [EDF.dll]\n");
        return 2;
    }
    const bool withGame = argc > 3;
    if (withGame && !fs::exists(argv[3])) {
        std::printf("SKIPPED: no EDF.dll (set EDF6_GAME_DIR)\n");
        return 77;
    }
    const fs::path plugins = fs::path(argv[2]) / "Mods" / "Plugins";
    fs::remove_all(argv[2]);
    fs::create_directories(plugins);
    const fs::path plugin = plugins / "EDF6AutoLoot.dll";
    fs::copy_file(argv[1], plugin);

    HMODULE game = nullptr;
    if (withGame) {
        game = LoadLibraryExA(argv[3], nullptr, DONT_RESOLVE_DLL_REFERENCES);
        if (!game) {
            std::printf("FAIL: could not map %s (error %lu)\n", argv[3], GetLastError());
            return 1;
        }
    }
    const HMODULE module = LoadLibraryW(plugin.c_str());
    Check(module != nullptr, "the plugin loads");
    if (!module) return 1;
    const auto load = reinterpret_cast<Load>(GetProcAddress(module, "EML6_Load"));
    Check(load != nullptr, "the plugin exports EML6_Load");
    if (!load) return 1;
    PluginInfo info{};
    const bool loaded = load(&info);
    Check(info.infoVersion == PluginInfo::MaxInfoVer && info.name && std::strcmp(info.name, "EDF6AutoLoot") == 0,
          "the plugin names itself to the loader");
    const std::string log = ReadText(plugins / "EDF6AutoLoot.log");

    if (!withGame) {
        Check(!loaded, "outside the game the plugin refuses");
        Check(log.find("REFUSED: EDF.dll is not loaded") != std::string::npos, "and its log says why");
        // EDFModLoader unloads a plugin that refuses: nothing of it may stay registered or open.
        Check(FreeLibrary(module) && !GetModuleHandleW(plugin.c_str()), "the refused plugin unloads");
        Check(fs::remove(plugins / "EDF6AutoLoot.log"), "its log is not held open");
    } else {
        Check(loaded, "the plugin installs against the supported EDF.dll");
        Check(log.find("Installed:") != std::string::npos, "and logs it");
        std::uintptr_t slot = 0;
        std::memcpy(&slot, reinterpret_cast<const unsigned char*>(game) + 0x1793088, sizeof(slot));
        Check(InModule(slot, module), "the mission exit slot leads into the plugin");
    }
    std::printf(failures ? "%d check(s) failed\n" : "all checks passed\n", failures);
    return failures ? 1 : 0;
}
