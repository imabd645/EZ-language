#ifndef EZLIB_PATH_H
#define EZLIB_PATH_H

#include <string>
#include <cstdlib>

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
        // POSIX fallback resolution order
        static const char* searchPaths[] = {
            "/usr/local/lib/ezlib",
            "/usr/lib/ezlib",
            "./lib/ezlib",
            "./ezlib"
        };
        base = searchPaths[0];
        for (const char* path : searchPaths) {
            if (FILE* f = fopen(path, "r")) {
                fclose(f);
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
