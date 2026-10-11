#!/usr/bin/env bash
# Builds EDF6AutoLoot.dll and its tests on Linux (clang for MinGW-w64, cmake/mingw-w64.cmake); -t runs the tests
# under Wine.
#   ./build.sh [-t] [-r]
# The plugin lands in out/. Set EDF6_GAME_DIR to the game folder (holds EDF.dll; only read) for the tests against
# the game's code; without it those tests report skipped. -r refuses to configure without EDF.dll.
# The tests run in their own Wine prefix (AUTOLOOT_WINEPREFIX, default ~/.local/share/wine-edf6autoloot-tests).
set -euo pipefail

root=$(cd "$(dirname "$0")" && pwd)
build=$root/build-mingw
test=0
require=OFF
while getopts tr option; do
    case $option in
        t) test=1 ;;
        r) require=ON ;;
        *) sed -n '2,8p' "$0"; exit 2 ;;
    esac
done

# Always explicit: CMake keeps cached values.
cmake -S "$root" -B "$build" -G Ninja -DCMAKE_TOOLCHAIN_FILE="$root/cmake/mingw-w64.cmake" \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo -DEDF6_REQUIRE_GAME=$require \
    "-DEDF6_GAME_DIR=${EDF6_GAME_DIR:-}"
cmake --build "$build"

if [ $test = 1 ]; then
    export WINEPREFIX=${AUTOLOOT_WINEPREFIX:-$HOME/.local/share/wine-edf6autoloot-tests}
    export WINEDEBUG=-all
    if [ ! -d "$WINEPREFIX" ]; then
        wineboot -i >/dev/null 2>&1
        # A test that crashes ends instead of waiting in Wine's debugger.
        wine reg add 'HKCU\Software\Wine\WineDbg' /v ShowCrashDialog /t REG_DWORD /d 0 /f >/dev/null
        wine reg delete 'HKLM\Software\Microsoft\Windows NT\CurrentVersion\AeDebug' /v Debugger /f >/dev/null 2>&1 || true
    fi
    ctest --test-dir "$build" --output-on-failure --no-tests=error --timeout 300
fi
