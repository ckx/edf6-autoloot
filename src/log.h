#pragma once

namespace autoloot {

// A short text log: a few lines at load and one per cleared mission. Every line opens, appends to and closes the
// file, so nothing stays open or running when the loader unloads a plugin that refused.
//
// Starts a new log at `path`, replacing the previous launch's.
void LogOpen(const wchar_t* path);
// One line, prefixed with the local time. Control characters are written as '?'.
void Log(const char* format, ...);

}  // namespace autoloot
