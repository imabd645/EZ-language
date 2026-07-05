cmake_minimum_required(VERSION 3.10)
project(EZLanguage CXX)

# Set the C++ standard to C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Add all source files based on the compiler command
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

# Create the executable named 'ez'
add_executable(ez ${SOURCES})

# Include directories
target_include_directories(ez PRIVATE src)

# Add compile definitions
target_compile_definitions(ez PRIVATE CURL_STATICLIB)

# Link options to enforce static linking of gcc and stdc++
target_link_options(ez PRIVATE 
    -static 
    -static-libgcc 
    -static-libstdc++ 
    -Wl,--subsystem,console
)

# Required libraries matching GCC command exactly
target_link_libraries(ez PRIVATE 
    sqlite3
    -Wl,-Bstatic curl ssh2 nghttp2 ngtcp2 ngtcp2_crypto_ossl nghttp3 brotlidec brotlicommon zstd idn2 psl iconv unistring ssl crypto z
    -Wl,-Bdynamic ws2_32 wldap32 bcrypt crypt32 secur32 iphlpapi
    dwmapi uxtheme gdi32 user32 comdlg32 comctl32 ole32
    pthread
)
