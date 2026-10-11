# Cross-build for Windows x64 from Linux: clang targeting x86_64-w64-mingw32 with MinGW-w64's headers and runtime,
# linked by lld. ctest runs the tests under Wine.
# Fedora: dnf install clang lld mingw64-gcc-c++ mingw64-winpthreads-static wine cmake ninja-build
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(MINGW_TARGET x86_64-w64-mingw32)
set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_CXX_COMPILER_TARGET ${MINGW_TARGET})
set(CMAKE_EXE_LINKER_FLAGS_INIT -fuse-ld=lld)
set(CMAKE_SHARED_LINKER_FLAGS_INIT -fuse-ld=lld)

set(CMAKE_FIND_ROOT_PATH /usr/${MINGW_TARGET}/sys-root/mingw)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

find_program(WINE wine REQUIRED)
set(CMAKE_CROSSCOMPILING_EMULATOR ${WINE})
