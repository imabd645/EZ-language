# BUILD.md - Build Instructions for EZ Language

## Prerequisites
## Never compile the interpreter Yourself
### Windows Environment

The EZ language interpreter is currently **Windows-only**. You need:

- **Windows 10 or later** (for console color support and modern Windows APIs)
- **MinGW-w64** with g++ (MSYS2 recommended)
  - Installation path: `C:\msys64\mingw64\`
  - Required: `g++.exe`, `ar.exe`, `windres.exe`
- **CMake 3.10+** (optional, for alternative build)
- **Git** (for cloning the repository)

### Required Libraries

All dependencies are included in the `dlls/` directory. No manual installation required if using the provided DLLs.

**Core Dependencies:**
- libffi - Foreign Function Interface
- libuv - Async I/O event loop
- libcurl - HTTP client
- sqlite3 - Database

**Windows DLLs** (25 files in `dlls/`):
- OpenSSL: `libssl-3-x64.dll`, `libcrypto-3-x64.dll`
- libcurl and dependencies: `libcurl-4.dll`, `libssh2.dll`, `libnghttp2.dll`, `libnghttp3.dll`, `libngtcp2.dll`, `libngtcp2_crypto_ossl.dll`
- Compression: `libzstd.dll`, `zlib1.dll`, `libbz2-1.dll`, `libbrotlidec.dll`, `libbrotlicommon.dll`
- Character encoding: `libiconv-2.dll`, `libunistring-5.dll`
- IDN: `libidn2-0.dll`, `libpsl-5.dll`
- Runtime: `libgcc_s_seh-1.dll`, `libstdc++-6.dll`, `libwinpthread-1.dll`
- Other: `libnghttp2-14.dll`

## Build Methods

### Method 1: Direct g++ Build (Primary)

**Command:**
```bash
build.bat
```

**What it does:**
- Uses `C:\msys64\mingw64\bin\g++.exe` directly
- Compiles all source files with aggressive optimization
- Statically links all dependencies
- Produces `ez.exe` in the current directory

**Output:**
- `ez.exe` (~17MB due to static linking)

**Requirements:**
- `dlls/` directory must be present with all 25 DLLs
- MinGW g++ at `C:\msys64\mingw64\bin\g++.exe`

**Build Flags:**
```
-O3 -march=native -flto -funroll-loops -fomit-frame-pointer
-static -static-libgcc -static-libstdc++ -Wl,--subsystem,console
-DCURL_STATICLIB
```

### Method 2: CMake Build (Alternative)

**Command:**
```bash
build_cmake.bat
```

**What it does:**
- Creates `build/` directory
- Configures with CMake (Release mode)
- Builds with 8 parallel jobs (`-j8`)
- Produces `build\ez.exe`

**Output:**
- `build/ez.exe`
- `build/CMakeCache.txt` and other CMake files

**Requirements:**
- CMake 3.10+ in PATH
- MinGW g++ at `C:\msys64\mingw64\bin\g++.exe`
- `dlls/` directory present

**CMake Configuration:**
```cmake
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -flto -funroll-loops -fomit-frame-pointer")
add_definitions(-DCURL_STATICLIB)
```

## Build File Details

### build.bat

**Location**: Repository root

**Key sections:**
```batch
@echo off
set CC=C:\msys64\mingw64\bin\g++.exe
set CFLAGS=-O3 -march=native -flto -funroll-loops -fomit-frame-pointer
set LDFLAGS=-static -static-libgcc -static-libstdc++ -Wl,--subsystem,console
set INCLUDES=-Isrc -IC:/msys64/mingw64/include
set LIBDIRS=-LC:/msys64/mingw64/lib
set LIBS=-lsqlite3 -lffi -luv -Wl,-Bstatic -lcurl -lssh2 -lnghttp2 ... -Wl,-Bdynamic -lws2_32 -lwldap32 ...
```

**Source files**: Explicitly lists all 34 .cpp files from `src/`

### CMakeLists.txt

**Location**: Repository root

**Key sections:**
```cmake
cmake_minimum_required(VERSION 3.10)
project(EZLanguage CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -flto -funroll-loops -fomit-frame-pointer")
add_definitions(-DCURL_STATICLIB)

include_directories(src)
include_directories(C:/msys64/mingw64/include)

# Source files explicitly listed
set(SOURCES
    src/main.cpp
    src/builtins/Builtins.cpp
    # ... 33 more files
)

add_executable(ez ${SOURCES})

target_link_libraries(ez PRIVATE sqlite3 ffi uv)
target_link_libraries(ez PRIVATE -Wl,-Bstatic curl ssh2 nghttp2 ...)
target_link_libraries(ez PRIVATE -Wl,-Bdynamic ws2_32 wldap32 ...)
```

## Running the Built Executable

### Basic Execution

**Run a script:**
```bash
ez.exe script.ez
```

**REPL mode:**
```bash
ez.exe
```

**Trace execution (debug):**
```bash
ez.exe -trace script.ez
```

### DLL Requirements

The `ez.exe` executable requires all DLLs from the `dlls/` directory to be present in the same directory as the executable or in the system PATH.

**Recommended structure:**
```
EZ-language/
├── ez.exe
├── dlls/
│   ├── libssl-3-x64.dll
│   ├── libcrypto-3-x64.dll
│   └── ... (23 more DLLs)
└── script.ez
```

**Alternative**: Copy all DLLs to the same directory as `ez.exe`.

## Troubleshooting

### Common Build Errors

**Error: "g++.exe not found"**
- Ensure MinGW is installed at `C:\msys64\mingw64\`
- Add `C:\msys64\mingw64\bin` to PATH temporarily

**Error: "undefined reference to ..."**
- Check that all DLLs are present in `dlls/`
- Verify library paths in `build.bat` or `CMakeLists.txt`

**Error: "cannot find -lssl" or similar**
- Ensure OpenSSL DLLs are in `dlls/`
- Check that library directory path is correct

**Error: "CMake not found"**
- Install CMake from https://cmake.org/download/
- Add CMake to system PATH

### Runtime Errors

**Error: "The program can't start because libssl-3-x64.dll is missing"**
- Ensure all DLLs are in the same directory as `ez.exe`
- Or add `dlls/` directory to PATH

**Error: "Access violation" during FFI calls**
- This is expected for invalid FFI operations
- The interpreter catches these via SEH and reports runtime errors

### Performance Issues

**Build is slow:**
- Use CMake build with `-j8` for parallel compilation
- Ensure antivirus is not scanning the build directory

**Executable is large (~17MB):**
- This is normal due to static linking
- Reduces deployment complexity (no DLL dependencies at runtime)

## Development Build

### Debug Build

To build with debug symbols (slower, but allows debugging):

**Modify build.bat:**
```batch
set CFLAGS=-g -O0
set LDFLAGS=-g
```

**Or modify CMakeLists.txt:**
```cmake
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
```

### Release Build (Default)

The default build uses aggressive optimization:
- `-O3`: Maximum optimization
- `-march=native`: Optimize for current CPU
- `-flto`: Link-time optimization
- `-funroll-loops`: Loop unrolling
- `-fomit-frame-pointer`: Omit frame pointers

## Clean Build

### Clean Direct Build

Delete `ez.exe` and rebuild:
```bash
del ez.exe
build.bat
```

### Clean CMake Build

Delete `build/` directory and rebuild:
```bash
rmdir /s /q build
build_cmake.bat
```

## Cross-Platform Notes

### Current Status

**Windows**: Fully supported (primary platform)
**Linux**: Not supported (hard Windows dependencies)
**macOS**: Not supported (hard Windows dependencies)

### Porting Challenges

To port EZ to other platforms, the following would need to be addressed:

1. **Console Functions**: Replace Windows console API with platform-specific equivalents
2. **FFI SEH**: Replace Windows SEH with Unix signal handlers
3. **Futures**: Replace Windows Events with pthread condition variables
4. **GUI**: Windows GUI builtins would need complete rewrite
5. **Paths**: Hardcoded "C:/ezlib" path needs platform detection
6. **DLL Loading**: Replace LoadLibrary with dlopen

### Platform-Specific Code Locations

- `src/builtins/Builtins_FFI.cpp`: Extensive `#ifdef _WIN32` and `#ifdef _MSC_VER`
- `src/builtins/Builtins_Console.cpp`: Windows-only console functions
- `src/builtins/Builtins_Net.cpp`: Winsock2 includes
- `src/eventloop/EventLoop.h`: Windows macro undefs for libuv
- `src/vm/BytecodeVM.cpp`: Windows console color support
- `src/compiler/BytecodeCompilerStmt.cpp`: Platform-specific ezlib paths

## Continuous Integration

**Status**: No CI/CD configuration found in repository.

**Recommendations**:
- Add GitHub Actions or Azure Pipelines for automated builds
- Run test suite on every commit
- Build both debug and release configurations
- Test on multiple Windows versions

## Testing the Build

### Run Test Suite

```bash
ez.exe Test/test.ez
```

### Run Specific Tests

```bash
ez.exe Test/test_buffer.ez
ez.exe Test/test_async_v2.ez
ez.exe Test/test_dict.ez
```

### Run Handwritten Tests

```bash
ez.exe Test/HandwrittenTests/arr_51_empty_array.ez
```

### Run Examples

```bash
ez.exe examples/test_math.ez
ez.exe examples/test_collections.ez
```

## Build System Architecture

### Source File Organization

**Total source files**: 34 .cpp files across 14 directories

```
src/
├── main.cpp
├── builtins/ (11 files)
│   ├── Builtins.cpp
│   ├── Builtins_Buffer.cpp
│   ├── Builtins_Concurrency.cpp
│   ├── Builtins_Console.cpp
│   ├── Builtins_Core.cpp
│   ├── Builtins_Data.cpp
│   ├── Builtins_FFI.cpp
│   ├── Builtins_GC.cpp
│   ├── Builtins_Http.cpp
│   ├── Builtins_IO.cpp
│   ├── Builtins_Math.cpp
│   ├── Builtins_Net.cpp
│   └── Builtins_String.cpp
├── bytecode/ (2 files)
├── cli/ (3 files)
├── compiler/ (3 files)
├── eventloop/ (1 file)
├── gc/ (1 file)
├── gui/ (5 files)
├── lexer/ (1 file)
├── parser/ (3 files)
├── runtime/ (1 file)
├── typechecker/ (3 files)
├── utils/ (1 file)
└── vm/ (3 files)
```

### Include Paths

**Primary**: `src/` (all headers)
**Secondary**: `C:/msys64/mingw64/include` (external libraries)

### Library Paths

**Primary**: `C:/msys64/mingw64/lib` (external libraries)

### Link Order

Static libraries first, then dynamic libraries:
```
sqlite3 ffi uv
curl ssh2 nghttp2 ... (static)
ws2_32 wldap32 bcrypt ... (dynamic Windows libs)
```

## Advanced Build Options

### Custom Compiler

To use a different compiler, modify `build.bat`:

```batch
set CC=C:\path\to\your\g++.exe
```

### Custom Optimization

To change optimization level, modify `CFLAGS` in `build.bat`:

```batch
set CFLAGS=-O2  # Less aggressive optimization
set CFLAGS=-Os  # Optimize for size
```

### Disable Static Linking

To allow dynamic linking (smaller executable, requires DLLs at runtime):

```batch
set LDFLAGS=-Wl,--subsystem,console
```

Remove `-static -static-libgcc -static-libstdc++` flags.

## Summary

- **Primary build**: `build.bat` (MinGW g++)
- **Alternative build**: `build_cmake.bat` (CMake)
- **Output**: `ez.exe` (~17MB)
- **Dependencies**: 25 DLLs in `dlls/` directory
- **Platform**: Windows-only
- **C++ Standard**: C++17
- **Optimization**: Aggressive (-O3 -march=native -flto)
- **Linking**: Static by default
