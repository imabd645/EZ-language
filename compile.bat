@echo off
echo Compiling EZ Interpreter...
g++ -std=c++17 -o ez.exe src\main.cpp src\Lexer.cpp src\Parser.cpp src\Interpreter.cpp src\Builtins.cpp -lsqlite3 -lcurl -lws2_32 -lpthread
if %errorlevel% neq 0 (
    echo Compilation failed!
    exit /b %errorlevel%
)
echo Compilation successful!
