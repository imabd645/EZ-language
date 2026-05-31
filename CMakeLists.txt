cmake_minimum_required(VERSION 3.10)
project(EZLanguage CXX)

# Set the C++ standard to C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Add all source files based on the compiler command
set(SOURCES
    src/main.cpp
    src/Lexer.cpp
    src/Parser.cpp
    src/Bytecode.cpp
    src/BytecodeCompiler.cpp
    src/BytecodeVM.cpp
    src/BytecodeInterpreter.cpp
    src/Builtins.cpp
    src/GUIBuiltins.cpp
    src/GC.cpp
    src/GCObject.cpp
    src/runtime/Builtins_IO.cpp
    src/runtime/Builtins_Math.cpp
    src/runtime/Builtins_Net.cpp
    src/runtime/Builtins_DB.cpp
    src/runtime/Builtins_String.cpp
    src/runtime/Builtins_Data.cpp
    src/runtime/Builtins_PDF.cpp
    src/runtime/Builtins_Sys.cpp
)

# Create the executable named 'ez'
add_executable(ez ${SOURCES})

# Include directories
target_include_directories(ez PRIVATE src)

# Link the required libraries identically to your GCC command: -lsqlite3 -lcurl -lws2_32 -lpthread
target_link_libraries(ez PRIVATE sqlite3 curl ws2_32 pthread)

# If compiling on Windows, ensure the GUI libraries are explicitly linked 
# (useful for GUIBuiltins.cpp if MinGW ignores #pragma comment)
if(WIN32)
    target_link_libraries(ez PRIVATE dwmapi uxtheme)
endif()
