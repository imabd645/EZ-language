#include "utils/EzLibPath.h"

#ifdef _WIN32
#include <windows.h>
// windows.h macros that conflict with EZ token names — clean up immediately.
#undef INTERFACE
#undef ERROR
#undef IN
#undef OUT
#undef TRUE
#undef FALSE
#else
#include <unistd.h>
#include <limits.h>
#endif

std::string ezExeDir() {
    // Cache: the exe directory never changes at runtime.
    static std::string cached;
    if (!cached.empty()) return cached;

    std::string exePath;
#ifdef _WIN32
    char buffer[4096];
    if (GetModuleFileNameA(NULL, buffer, sizeof(buffer))) {
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
            cached = exePath.substr(0, lastSlash + 1);
            return cached;
        }
    }
    cached = "./";
    return cached;
}

std::string ezLibBase() {
    static std::string cached;
    if (!cached.empty()) return cached;
    cached = ezExeDir() + "lib/";
    return cached;
}

std::string ezLibBaseNoSlash() {
    static std::string cached;
    if (!cached.empty()) return cached;
    cached = ezExeDir() + "lib";
    return cached;
}
