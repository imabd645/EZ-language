#ifndef EZLIB_PATH_H
#define EZLIB_PATH_H

#include <string>
#include <cstdlib>
#ifndef _WIN32
#include <sys/stat.h>
#endif

#ifdef _WIN32
extern "C" __declspec(dllimport) unsigned long __stdcall GetModuleFileNameA(void* hModule, char* lpFilename, unsigned long nSize);
#else
#include <unistd.h>
#include <limits.h>
#endif

// Resolve the standard-library base directory.
//
// Honours the EZLIB_PATH environment variable so the stdlib can live anywhere
// (custom install, non-admin user, portable checkout, CI), falling back to the
// platform default.
//
// The result always ends in a separator.
inline std::string ezLibBase() {
    std::string base;
    if (const char* env_p = std::getenv("EZLIB_PATH")) {
        base = env_p;
    } else {
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
                base = exePath.substr(0, lastSlash) + "/lib";
            } else {
                base = "lib";
            }
        } else {
            base = "lib"; // Fallback
        }
    }
    if (!base.empty() && base.back() != '/' && base.back() != '\\') {
        base += "/";
    }
    return base;
}

// Same as ezLibBase() but without the trailing separator, for callers that
// build their own paths (e.g. baseDir + "/.cache").
inline std::string ezLibBaseNoSlash() {
    std::string base = ezLibBase();
    if (!base.empty() && (base.back() == '/' || base.back() == '\\')) {
        base.pop_back();
    }
    return base;
}

#endif // EZLIB_PATH_H
