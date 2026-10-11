#include <windows.h>

#pragma warning(push)
#pragma warning(disable : 4201)
#include "PluginAPI.h"
#pragma warning(pop)

#include "autoloot.h"
#include "log.h"

namespace {

#define AUTOLOOT_TEXT2(x) #x
#define AUTOLOOT_TEXT(x) AUTOLOOT_TEXT2(x)
constexpr const char* kVersion = AUTOLOOT_TEXT(AUTOLOOT_VERSION_MAJOR) "." AUTOLOOT_TEXT(
    AUTOLOOT_VERSION_MINOR) "." AUTOLOOT_TEXT(AUTOLOOT_VERSION_PATCH);

HMODULE self = nullptr;

// <plugin folder>\EDF6AutoLoot.log
bool LogPath(wchar_t (&path)[MAX_PATH]) {
    const DWORD length = GetModuleFileNameW(self, path, MAX_PATH);
    if (!length || length >= MAX_PATH - 4) return false;
    wchar_t* dot = nullptr;
    for (wchar_t* at = path; *at; ++at)
        if (*at == L'.') dot = at;
        else if (*at == L'\\' || *at == L'/') dot = nullptr;
    if (!dot) return false;
    lstrcpyW(dot, L".log");
    return true;
}

}  // namespace

// EDFModLoader calls this once at startup. False unloads the plugin at once; true keeps it for the life of the
// process, since the game's mission exit slot then points into it.
extern "C" __declspec(dllexport) bool EDFMLAPI EML6_Load(PluginInfo* info) {
    if (!info) return false;
    info->infoVersion = PluginInfo::MaxInfoVer;
    info->name = "EDF6AutoLoot";
    info->version = PLUG_VER(AUTOLOOT_VERSION_MAJOR, AUTOLOOT_VERSION_MINOR, AUTOLOOT_VERSION_PATCH, 0);
    wchar_t logPath[MAX_PATH]{};
    if (LogPath(logPath)) autoloot::LogOpen(logPath);
    autoloot::Log("==== EDF6AutoLoot %s ====", kVersion);
    const HMODULE game = GetModuleHandleW(L"EDF.dll");
    if (!game) {
        autoloot::Log("REFUSED: EDF.dll is not loaded, so this is not the game; nothing was changed");
        return false;
    }
    return autoloot::InstallMissionClearHook(reinterpret_cast<unsigned char*>(game));
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        self = instance;
        DisableThreadLibraryCalls(instance);
    }
    return TRUE;
}
