#ifndef EZLIB_PATH_H
#define EZLIB_PATH_H

#include <string>

#ifdef _WIN32
extern "C" __declspec(dllimport) unsigned long __stdcall GetModuleFileNameA(void* hModule, char* lpFilename, unsigned long nSize);
#else
#include <unistd.h>
#include <limits.h>
#endif

// ── Interpreter-relative path resolution ─────────────────────────────────────
//
// Python-style: the standard library and all installed packages live in a
// single `lib/` directory next to the interpreter executable. There is no
// environment variable override and no CWD-relative search — one interpreter,
// one library folder.
//
//     <exe_dir>/
//     ├── ez.exe          (or ez on POSIX)
//     └── lib/
//         ├── sqlite/
//         ├── http/
//         ├── testing/
//         └── ...

// Return the directory containing the running ez executable.
// Result always ends with a separator ('/' or '\\').
inline std::string ezExeDir() {
    std::string exePath;
#ifdef _WIN32
    char buffer[4096];
    if (GetModuleFileNameA(nullptr, buffer, sizeof(buffer))) {
        exePath = buffer;
    }
#else
    char buffer[PATH_MAX];
    ssize_t count = readlink("/proc/self/exe", buffer, PATH_MAX);
    if (count > 0) {
        exePath = std::string(buffer, count);
    }
#endif
    if (!exePath.empty()) {
        size_t lastSlash = exePath.find_last_of("/\\");
        if (lastSlash != std::string::npos) {
            return exePath.substr(0, lastSlash + 1);
        }
    }
    return "./";
}

// Resolve the standard-library base directory: <exe_dir>/lib/
// The result always ends in a separator.
inline std::string ezLibBase() {
    return ezExeDir() + "lib/";
}

// Same as ezLibBase() but without the trailing separator, for callers that
// build their own paths (e.g. baseDir + "/.cache").
inline std::string ezLibBaseNoSlash() {
    return ezExeDir() + "lib";
}

#endif // EZLIB_PATH_H
