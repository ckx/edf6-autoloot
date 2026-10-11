#include "log.h"

#include <windows.h>

#include <cstdarg>
#include <cstdio>

namespace autoloot {
namespace {

wchar_t logPath[MAX_PATH]{};
SRWLOCK logLock = SRWLOCK_INIT;

void Append(const char* text, DWORD length, DWORD disposition) {
    if (!logPath[0]) return;
    const HANDLE file = CreateFileW(logPath, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                    disposition, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return;
    DWORD written = 0;
    WriteFile(file, text, length, &written, nullptr);
    CloseHandle(file);
}

}  // namespace

void LogOpen(const wchar_t* path) {
    AcquireSRWLockExclusive(&logLock);
    lstrcpynW(logPath, path, MAX_PATH);
    Append("", 0, CREATE_ALWAYS);
    ReleaseSRWLockExclusive(&logLock);
}

void Log(const char* format, ...) {
    SYSTEMTIME now{};
    GetLocalTime(&now);
    char line[512];
    int length = std::snprintf(line, sizeof(line), "%02u:%02u:%02u ", now.wHour, now.wMinute, now.wSecond);
    va_list args;
    va_start(args, format);
    const int body = std::vsnprintf(line + length, sizeof(line) - length - 2, format, args);
    va_end(args);
    if (body < 0) return;
    const int end = length + body;
    length = end < static_cast<int>(sizeof(line)) - 2 ? end : static_cast<int>(sizeof(line)) - 3;
    for (int i = 0; i < length; ++i)
        if (static_cast<unsigned char>(line[i]) < 0x20) line[i] = '?';
    line[length++] = '\r';
    line[length++] = '\n';
    AcquireSRWLockExclusive(&logLock);
    Append(line, static_cast<DWORD>(length), OPEN_ALWAYS);
    ReleaseSRWLockExclusive(&logLock);
}

}  // namespace autoloot
