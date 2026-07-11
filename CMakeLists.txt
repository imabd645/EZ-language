cmake_minimum_required(VERSION 3.10)
project(EZLanguage CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Optimization flags (matching build.bat)
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -flto -funroll-loops -fomit-frame-pointer")

# Defines
add_definitions(-DCURL_STATICLIB)

# Include directories
include_directories(src)
include_directories(C:/msys64/mingw64/include)

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
target_link_libraries(ez PRIVATE sqlite3 ffi)

# Static libraries (curl and dependencies)
target_link_libraries(ez PRIVATE
    -Wl,-Bstatic curl ssh2 nghttp2 ngtcp2 ngtcp2_crypto_ossl nghttp3 brotlidec brotlicommon zstd idn2 psl iconv unistring ssl crypto z
)

# Dynamic Windows libraries
target_link_libraries(ez PRIVATE
    -Wl,-Bdynamic ws2_32 wldap32 bcrypt crypt32 secur32 iphlpapi
    dwmapi uxtheme gdi32 user32 comdlg32 comctl32 ole32
    pthread
)
