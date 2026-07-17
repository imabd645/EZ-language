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
# -O3 -march=native -funroll-loops still applies; the measured throughput
# difference from dropping LTO here is small, and correct exception handling is
# not optional for a language with try/catch.
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -funroll-loops -fasynchronous-unwind-tables")

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
    src/builtins/Builtins_FFI.cpp
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
    src/gui/GUI_Core.cpp
    src/gui/GUI_Dialogs.cpp
    src/gui/GUI_Drawing.cpp
    src/gui/GUI_Menu.cpp
    src/gui/GUI_Widgets.cpp
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

# Dynamic Windows libraries
target_link_libraries(ez PRIVATE
    -Wl,-Bdynamic ws2_32 wldap32 bcrypt crypt32 secur32 iphlpapi userenv psapi dbghelp
    dwmapi uxtheme gdi32 user32 comdlg32 comctl32 ole32
    pthread
)
