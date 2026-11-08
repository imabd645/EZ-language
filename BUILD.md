# EZ Language Build and Deployment

## Supported platforms

The interpreter core is cross-platform. The native `gui_*` framework and
Windows-specific FFI integrations require Windows.

| Platform | Interpreter | Native GUI |
|---|---|---|
| Windows 10/11 | Supported | Supported |
| Linux | Supported | Not supported |
| macOS | Supported | Not supported |

## Prerequisites

### Windows

- CMake 3.10 or newer
- MinGW-w64 g++ (MSYS2 recommended)
- The MSYS2 MinGW prefix, normally `C:\msys64\mingw64`
- Git

The build links against libffi, libuv, libcurl, and SQLite. On Windows the
expected headers and libraries are normally installed in the MSYS2 MinGW
prefix. The `dlls/` directory contains optional runtime dependencies used by
older or dynamically linked distributions; the current release configuration
statically links the C++ runtime and bundled third-party libraries where
available.

### Linux and macOS

Install the development packages for:

- libffi
- libuv
- libcurl
- SQLite
- OpenSSL

On macOS, Homebrew installations under `/opt/homebrew` and `/usr/local` are
recognized by `CMakeLists.txt`.

## Canonical build

CMake is the single source of truth for the source list, compiler flags, link
order, platform libraries, and standard-library deployment.

### Windows batch entry points

Both batch files configure and build the same CMake project:

```bat
build.bat
build_cmake.bat
```

The output is:

```text
build\ez.exe
build\lib\
```

`build.bat` is retained as a compatibility entry point; it no longer
duplicates a separate g++ source list.

Optional environment variables:

```bat
set EZ_ARCH=native
set MSYS2_PREFIX=C:\msys64\mingw64
build.bat
```

The default release target is `x86-64-v2`, which is suitable for current
Windows 10/11 x64 machines. Use `EZ_ARCH=native` only for a local binary that
will not be distributed.

### Direct CMake commands

```powershell
cmake -S . -B build -G "MinGW Makefiles" `
  -DCMAKE_BUILD_TYPE=Release `
  -DEZ_ARCH=x86-64-v2 `
  -DMSYS2_PREFIX=C:/msys64/mingw64
cmake --build build --parallel
```

For a debug build:

```powershell
cmake -S . -B build-debug -G "MinGW Makefiles" `
  -DCMAKE_BUILD_TYPE=Debug `
  -DMSYS2_PREFIX=C:/msys64/mingw64
cmake --build build-debug --parallel
```

### Build configuration

The release configuration uses:

- C++17
- `-O3`
- `-funroll-loops`
- `-fasynchronous-unwind-tables`
- A configurable CPU baseline through `EZ_ARCH`

Link-time optimization and `-fomit-frame-pointer` are intentionally disabled
because they previously made C++ exception unwinding unreliable through the
computed-goto VM dispatch and libuv callback paths.

## Running the interpreter

```powershell
.\build\ez.exe --version
.\build\ez.exe script.ez
.\build\ez.exe -c 'out "Hello"'
.\build\ez.exe --trace script.ez
.\build\ez.exe
```

Keep the `lib\` directory beside `ez.exe`; CMake copies it as a post-build
step. Bundled applications also need the standard library embedded or
available according to the bundling workflow.

## Testing

Run focused tests with the freshly built executable:

```powershell
.\build\ez.exe Test\test_builtin.ez
.\build\ez.exe Test\test_control_flow_complex_jumps.ez
.\build\ez.exe Test\test_exceptions.ez
.\build\ez.exe Test\test_async_v2.ez
.\build\ez.exe Test\test_ffi_safety.ez
.\build\ez.exe Test\test_gc_threadsafe.ez
```

The repository contains both integration tests in `Test\` and handwritten
tests in `Test\HandwrittenTests\`. `Test\test.ez` is a small language/type
checker fixture rather than the aggregate test runner; it intentionally
contains an undefined-variable diagnostic and is not a full-suite command.

When adding a new test that calls a builtin, follow the syntax documented in
[BUILTINS.md](BUILTINS.md).

## Clean builds

```powershell
Remove-Item -Recurse -Force build
.\build_cmake.bat
```

Use a separate build directory such as `build-debug` when you need to keep a
release build intact.

## Deployment checklist

1. Build with the default `x86-64-v2` release configuration.
2. Verify `.\build\ez.exe --version`.
3. Keep `build\lib\` beside the executable.
4. Run representative control-flow, exception, async, FFI, and GC tests.
5. For untrusted scripts, use `--safe --no-prompt` and grant only required
   capabilities.
6. For a standalone application, use `ez bundle` and test the generated
   executable on a clean machine.

## Source organization

The current tree contains approximately 63 `.cpp` files under `src\`:

```text
src\
├── ast\
├── builtins\
│   ├── buffer\
│   ├── core\
│   ├── data\
│   ├── FFI\
│   ├── io\
│   ├── math\
│   ├── net\
│   ├── string\
│   └── time\
├── bytecode\
├── cli\
├── compiler\
├── eventloop\
├── gc\
├── gui\
├── lexer\
├── parser\
├── runtime\
├── testing\
├── typechecker\
├── utils\
└── vm\
```

When adding a C++ source file, add it to `CMakeLists.txt`. Do not maintain a
second manual source list in a batch file.

## Troubleshooting

### CMake cannot find MinGW libraries

Set the MSYS2 prefix explicitly:

```bat
set MSYS2_PREFIX=C:\msys64\mingw64
build_cmake.bat
```

### CMake generator mismatch

Use a fresh build directory after switching generators:

```powershell
Remove-Item -Recurse -Force build
.\build_cmake.bat
```

### FFI access violation

Invalid native calls are reported as catchable EZ errors when they pass through
the guarded FFI entry points. Raw pointers returned by external libraries are
not bounds-tracked; callers remain responsible for their lifetime and declared
signatures.
