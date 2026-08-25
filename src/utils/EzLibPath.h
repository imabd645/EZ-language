#ifndef EZLIB_PATH_H
#define EZLIB_PATH_H

#include <string>

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
// Result always ends with a '/'.
std::string ezExeDir();

// Resolve the standard-library base directory: <exe_dir>/lib/
// The result always ends in a '/'.
std::string ezLibBase();

// Same as ezLibBase() but without the trailing separator, for callers that
// build their own paths (e.g. baseDir + "/.cache").
std::string ezLibBaseNoSlash();

#endif // EZLIB_PATH_H
