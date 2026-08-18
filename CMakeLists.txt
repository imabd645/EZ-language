cmake_minimum_required(VERSION 3.10)
project(EZLanguage CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Optimization flags.
#
# -flto and -fomit-frame-pointer were REMOVED, and -fasynchronous-unwind-tables
# added, to make C++ exception unwinding reliable.
#
# Symptom: an EZ runtime error (e.g. indexing nil) inside an FFI callback that
# runs via the libuv event loop -- exactly how every ezweb request handler is
# invoked -- crashed the process with an access violation, even though the error
# was caught in EZ. gdb showed the C++ exception thrown by runtimeError()
# unwinding straight past run()'s and callFunction()'s catch handlers (both were
# skipped) up to a null landing pad, with LTO clones ('.lto_priv.0') on the
# stack. LTO had produced private clones of the exception path with broken
# unwind/landing-pad tables, so the personality routine could not find the
# handler when the throw originated deep in the computed-goto dispatch under the
# event-loop call chain. It worked synchronously only because that path used the
# un-cloned functions.
#
# -O3 -funroll-loops still applies; the measured throughput difference from
# dropping LTO here is small, and correct exception handling is not optional
# for a language with try/catch.

# CPU baseline for the produced binary.
#
# This was -march=native, which tunes for the CPU of the machine running the
# build and emits instructions older CPUs lack. Such a binary runs locally and
# dies with an illegal-instruction crash on any older machine, before main()
# and with no diagnostic -- invisible until a user reports it. The default is
# therefore a baseline, not the build host.
#
# x86-64-v2 = SSE3/SSSE3/SSE4.1/SSE4.2/POPCNT: Intel Nehalem (2008) and AMD
# Bulldozer (2011) onward, covering every machine that runs Windows 10 or 11.
#
# For a local, non-distributed build:  cmake -DEZ_ARCH=native ..
set(EZ_ARCH "x86-64-v2" CACHE STRING
    "Target CPU for -march. Use 'native' only for builds you will not distribute.")

if(CMAKE_SYSTEM_PROCESSOR MATCHES "(x86_64|AMD64|amd64)")
    message(STATUS "Building for -march=${EZ_ARCH}")
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=${EZ_ARCH} -funroll-loops -fasynchronous-unwind-tables")
else()
    message(STATUS "Building for ${CMAKE_SYSTEM_PROCESSOR} (skipping -march=${EZ_ARCH})")
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -funroll-loops -fasynchronous-unwind-tables")
endif()

# Defines
add_definitions(-DCURL_STATICLIB)

# Include directories
include_directories(src)

# MinGW/MSYS2 prefix. Hardcoding C:/msys64 meant the build only worked on a
# machine with MSYS2 installed at exactly that path; override with
#   cmake -DMSYS2_PREFIX=D:/msys64/mingw64 ..
# or set the MSYS2_PREFIX environment variable.
if(NOT DEFINED MSYS2_PREFIX)
    if(DEFINED ENV{MSYS2_PREFIX})
        set(MSYS2_PREFIX $ENV{MSYS2_PREFIX})
    else()
        set(MSYS2_PREFIX "C:/msys64/mingw64")
    endif()
endif()
if(EXISTS "${MSYS2_PREFIX}/include")
    include_directories("${MSYS2_PREFIX}/include")
    link_directories("${MSYS2_PREFIX}/lib")
    message(STATUS "Using MinGW prefix: ${MSYS2_PREFIX}")
else()
    message(WARNING "MSYS2_PREFIX '${MSYS2_PREFIX}' has no include/ directory; "
                    "set -DMSYS2_PREFIX=<path-to>/mingw64 if the build cannot find libffi/libuv/curl.")
endif()

# Gather all source files explicitly
set(SOURCES
    src/main.cpp
    src/builtins/Builtins.cpp
    src/builtins/Builtins_Buffer.cpp
    src/builtins/Builtins_Concurrency.cpp
    src/builtins/Builtins_Console.cpp
    src/builtins/Builtins_Core.cpp
    src/builtins/Builtins_Data.cpp
    src/builtins/FFI/FFI.cpp
    src/builtins/FFI/FFI_Support.cpp
    src/builtins/FFI/FFI_Memory.cpp
    src/builtins/FFI/FFI_Call.cpp
    src/builtins/FFI/FFI_Callback.cpp
    src/builtins/FFI/FFI_Struct.cpp
    src/builtins/Builtins_GC.cpp
    src/builtins/Builtins_Http.cpp
    src/builtins/Builtins_IO.cpp
    src/builtins/Builtins_Math.cpp
    src/builtins/Builtins_Net.cpp
    src/builtins/Builtins_String.cpp
    src/builtins/Builtins_TimeDate.cpp
    src/bytecode/Bytecode.cpp
    src/bytecode/serializer/BytecodeSerializer.cpp
    src/cli/CLI.cpp
    src/cli/Packager.cpp
    src/cli/REPL.cpp
    src/compiler/BytecodeCompiler.cpp
    src/compiler/BytecodeCompilerExpr.cpp
    src/compiler/BytecodeCompilerStmt.cpp
    src/eventloop/EventLoop.cpp
    src/gc/CycleCollector.cpp
    src/lexer/Lexer.cpp
    src/parser/Parser.cpp
    src/parser/ParserExpr.cpp
    src/parser/ParserStmt.cpp
    src/runtime/Runtime.cpp
    src/typechecker/TypeChecker.cpp
    src/typechecker/TypeCheckerExpr.cpp
    src/typechecker/TypeCheckerStmt.cpp
    src/vm/BytecodeVM.cpp
    src/vm/BytecodeVM_Execute.cpp
    src/vm/BytecodeVM_Objects.cpp
)

# Create the executable
add_executable(ez ${SOURCES})

if(WIN32)
    # Static linking flags
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static -static-libgcc -static-libstdc++ -Wl,--subsystem,console")

    # Link directories
    link_directories(C:/msys64/mingw64/lib)

    # Basic libraries
    target_link_libraries(ez PRIVATE sqlite3 ffi uv)

    # Static libraries (curl and dependencies)
    target_link_libraries(ez PRIVATE
        -Wl,-Bstatic curl ssh2 nghttp2 ngtcp2 ngtcp2_crypto_ossl nghttp3 brotlidec brotlicommon zstd idn2 psl iconv unistring ssl crypto z
    )

    # Dynamic Windows libraries (Windows only)
    target_link_libraries(ez PRIVATE
        -Wl,-Bdynamic ws2_32 wldap32 bcrypt crypt32 secur32 iphlpapi userenv psapi dbghelp
        dwmapi uxtheme gdi32 user32 comdlg32 comctl32 ole32
    )
    
    # Add GUI sources
    target_sources(ez PRIVATE
        src/gui/GUI_Core.cpp
        src/gui/GUI_Dialogs.cpp
        src/gui/GUI_Drawing.cpp
        src/gui/GUI_Menu.cpp
        src/gui/GUI_Widgets.cpp
    )

    # MUST STAY LAST. g++ appends libstdc++ and libwinpthread after everything
    # listed here, so whichever -B mode the link line ends in decides how they
    # resolve. Ending on the -Wl,-Bdynamic block above silently defeated the
    # -static-libstdc++ in CMAKE_EXE_LINKER_FLAGS, and ez.exe imported
    # libstdc++-6.dll and libwinpthread-1.dll -- so every built .exe, bundled ones
    # included, needed those two DLLs beside it to start.
    # Verify after changing anything here:
    #   objdump -p ez.exe | grep -i "DLL Name"
    # Only Windows system DLLs should be listed.
    target_link_libraries(ez PRIVATE
        -Wl,-Bstatic stdc++ winpthread
    )
else()
    target_link_libraries(ez PRIVATE sqlite3 ffi uv curl ssl crypto dl pthread)
endif()
