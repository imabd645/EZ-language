#ifndef EZLIB_PATH_H
#define EZLIB_PATH_H

#include <string>
#include <cstdlib>
#ifndef _WIN32
#include <sys/stat.h>
#endif

// Resolve the standard-library base directory.
//
// Honours the EZLIB_PATH environment variable so the stdlib can live anywhere
// (custom install, non-admin user, portable checkout, CI), falling back to the
// platform default.
//
// This exists because the base path used to be resolved in three places: the
// compiler honoured EZLIB_PATH, but the bundler (Packager) and the package
// manager both hardcoded "C:/ezlib". That meant `ez run` could resolve a module
// from a custom EZLIB_PATH while `ez bundle` / `ez install` looked somewhere
// else entirely. Everything now goes through this one function.
//
// The result always ends in a separator.
inline std::string ezLibBase() {
    std::string base;
    if (const char* env_p = std::getenv("EZLIB_PATH")) {
        base = env_p;
    } else {
#ifdef _WIN32
        base = "C:/ezlib";
#else
        // POSIX fallback resolution order: first entry that exists as a
        // directory wins. If none do, keep the first so diagnostics name the
        // canonical install location rather than a relative path.
        //
        // stat() rather than fopen(): fopen() on a directory SUCCEEDS on Linux
        // and macOS (open(O_RDONLY) on a directory is permitted), so it cannot
        // tell "ezlib is installed here" from "something with that name
        // exists", and its behaviour on directories is not portable across
        // libc implementations anyway.
        static const char* searchPaths[] = {
            "/usr/local/lib/ezlib",
            "/usr/lib/ezlib",
            "./lib/ezlib",
            "./ezlib"
        };
        base = searchPaths[0];
        for (const char* path : searchPaths) {
            struct stat st;
            if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
                base = path;
                break;
            }
        }
#endif
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
