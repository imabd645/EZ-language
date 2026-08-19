# Getting Started

## Prerequisites

EZ's **core language is cross-platform**: the lexer, parser, static type
checker, bytecode compiler, VM, garbage collector, async event loop
(libuv), threading primitives, standard builtins, streaming file I/O, HTTP
client, SQLite/FFI layer all build and run on Windows, Linux, and macOS —
`CMakeLists.txt` has a dedicated non-Windows link path
(`sqlite3 ffi uv curl ssl crypto dl pthread`) and none of `src/lexer/`,
`src/parser/`, `src/compiler/`, `src/vm/`, `src/gc/`, `src/typechecker/`, or
`src/eventloop/` contain any Windows-specific code.

**The native GUI framework (`gui_*` builtins, `src/gui/`) is Windows-only** —
it's built directly on Win32/GDI and its sources are only compiled into the
executable when `WIN32` is set. The core interpreter and all other builtins are fully cross-platform.

| Dependency | Purpose |
|---|---|
| C++17 compiler (MinGW-w64/MSVC on Windows; GCC/Clang on Linux/macOS) | Building the interpreter |
| CMake 3.10+ | Build system |
| libcurl | HTTP client (`http_get`, `http_post`, `fetch`) |
| libsqlite3 | Linked by CMake; backs `@persist` and is available to `ezlib` packages via FFI (there are no `db_*` builtins in C++) |
| libffi | FFI callback trampolines |
| libuv | The async event loop |
| Win32 SDK (`dwmapi`, `uxtheme`) | Windows only — GUI dark-mode/theme APIs |

## Building from source

### On Windows

```bash
git clone https://github.com/imabd645/EZ-language.git
cd EZ-language

mkdir build && cd build
cmake .. -G "MinGW Makefiles"
cmake --build .
```

This produces `ez.exe`, statically linked against the bundled dependency
DLLs and dynamically against system libraries (`ws2_32`, `dwmapi`,
`uxtheme`, `gdi32`, `user32`, etc. — see `CMakeLists.txt`'s `WIN32` branch),
and includes the `src/gui/*.cpp` sources.

Alternatively, the repository ships a direct build script:

```bash
build.bat
```

This invokes the compiler directly (`C:\msys64\mingw64\bin\g++.exe` by
default) and statically links all dependencies to produce a single
`ez.exe`.

### On Linux / macOS

```bash
git clone https://github.com/imabd645/EZ-language.git
cd EZ-language

mkdir build && cd build
cmake ..
cmake --build . -j$(nproc)
```

`CMakeLists.txt`'s non-Windows branch links `sqlite3 ffi uv curl ssl crypto
dl pthread` from your system package manager (`apt`/`dnf`/Homebrew) and
skips the `src/gui/` sources entirely — the resulting `ez` binary runs
scripts, the REPL, async/concurrency, FFI, networking, and the whole
standard builtin surface identically to Windows. Calling a `gui_*` builtin
on this build raises an "undefined variable/function" runtime error rather
than a build failure, since GUI registration is compiled out.

> Build script and flag details (including historical notes on why
> `-flto`/`-fomit-frame-pointer` were dropped from the release flags to keep
> C++ exception unwinding reliable across the libuv event loop) are in
> `BUILD.md` and the comments at the top of `CMakeLists.txt`; treat
> `CMakeLists.txt` itself as the source of truth if the two disagree.

## Adding `ez` to your PATH

1. Move `ez.exe` to a permanent folder, e.g. `C:\ez\`.
2. Open **System Properties → Environment Variables → Path → Edit → New**.
3. Add `C:\ez`.
4. Restart your terminal.

```bash
ez --help             # show usage
ez hello.ez           # run a script
ez --trace hello.ez   # run with bytecode execution tracing
ez                    # start the REPL
```

## Your first script

```ez
out "Hello, World!"
```

```bash
ez hello.ez
# Hello, World!
```

## Five minutes of EZ

```ez
# Variables — no declaration keyword needed
name = "Abdullah"
age  = 19
pi   = 3.14159
active = true

out "Name: " + name
out "Age:  " + str(age)

# Conditional
when age >= 18 {
    out name + " is an adult"
} other {
    out name + " is a minor"
}

# repeat i = N to M — inclusive on both ends, auto-detects reverse direction
repeat i = 1 to 5 {
    out "Count: " + str(i)
}

# For-each loop
fruits = ["apple", "banana", "cherry"]
get fruit in fruits {
    out fruit
}

# Function
task square(n) {
    give n * n
}
out str(square(7))    # 49

# Lambda
double = |x| x * 2
out str(double(5))    # 10
```

## Tooling

### REPL

Running `ez` with no arguments starts an interactive session:

```
EZ Language Interpreter v1.0 (Bytecode Mode)
Type 'exit' to quit
>>> x = 5
>>> out x * 2
10
>>> exit
Goodbye!
```

The REPL is multi-line aware — it counts `{`/`}` and keeps prompting with
`...` until braces balance, then compiles and runs the accumulated input,
sharing global state across evaluations within the same session.

### CLI reference

| Command | Description |
|---|---|
| `ez` | Run REPL (interactive mode) |
| `ez <file.ez> [--trace]` | Run a script file, optionally tracing bytecode execution |
| `ez install <pkg> [version]` | Install a package from the `ezlib` registry into `C:/ezlib/` |
| `ez list` | List installed packages |
| `ez init <name>` | Scaffold a new package (`main.ez` + `package.ez`) |
| `ez bundle <file.ez> [out.exe] [--gui] [--icon app.ico]` | Create a standalone executable |
| `ez --help` / `-h` | Show usage |

### Bundling a standalone executable

```bash
ez bundle main.ez app.exe --gui --icon app.ico
```

What happens under the hood:

1. The entry script is lexed and every `use "path"` statement is resolved
   (against `lib/`, `C:/ezlib/`, and literal paths); the dependency graph is
   crawled recursively.
2. Every discovered `.ez` file (plus the entry script, renamed
   `__main__.ez`) is packed into an in-memory virtual file system blob:
   `[fileCount][nameLen][name][contentLen][content]...`.
3. The current `ez.exe` is copied to the output path.
4. If `--icon app.ico` is given, the icon is injected into the `.exe`'s PE
   resources.
5. The VFS blob is appended to the `.exe`, followed by a 4-byte size and the
   6-byte magic marker `EZPKV1`.
6. If `--gui` is given, the PE header's `Subsystem` field is patched from
   console (3) to GUI (2), hiding the console window at runtime.

At startup, `ez.exe` checks itself for the trailing `EZPKV1` marker; if
present, it loads the embedded VFS and runs `__main__.ez` directly instead
of starting the REPL or reading `argv[1]` as a script path.

### Installing an `ezlib` package

```bash
ez install collections
```

```ez
use "collections"
```

See [standard-library.md](standard-library.md) for the package ecosystem.

## Where to go next

- [syntax.md](syntax.md) — the complete grammar, keyword, and operator
  reference.
- [features.md](features.md) — a feature-by-feature tour.
- [api.md](api.md) — the full builtin function catalog.
- [object-oriented-programming.md](object-oriented-programming.md) — models,
  interfaces, structs, enums, operator overloading.
