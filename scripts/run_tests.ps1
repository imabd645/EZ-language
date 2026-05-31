# EZ Test Runner Automation Script
# Usage: .\scripts\run_tests.ps1

Write-Host "Starting EZ Language Test Suite..." -ForegroundColor Cyan

# 1. Recompile (optional but recommended to ensure latest builtins like panic are present)
Write-Host "[1/2] Recompiling EZ Interpreter..." -ForegroundColor Yellow
g++ -o ez.exe src/main.cpp src/Lexer.cpp src/Parser.cpp src/Bytecode.cpp src/BytecodeCompiler.cpp src/BytecodeVM.cpp src/BytecodeInterpreter.cpp src/Builtins.cpp src/GUIBuiltins.cpp src/GC.cpp src/GCObject.cpp src/runtime/Builtins_IO.cpp src/runtime/Builtins_Math.cpp src/runtime/Builtins_Net.cpp src/runtime/Builtins_DB.cpp src/runtime/Builtins_String.cpp src/runtime/Builtins_Data.cpp src/runtime/Builtins_PDF.cpp src/runtime/Builtins_Sys.cpp -lws2_32 -lsqlite3 -lcurl -lpthread -ldwmapi -luxtheme -lgdi32 -luser32 -lcomdlg32 -lcomctl32 -lcrypt32 -lole32

if ($LASTEXITCODE -ne 0) {
    Write-Host "CRITICAL: Compilation failed. Aborting tests." -ForegroundColor Red
    exit 1
}

# 2. Run Test Discovery Script
Write-Host "[2/2] Discovering and Running Tests..." -ForegroundColor Yellow
.\ez.exe scripts/test_runner.ez

Write-Host "`nTest execution process initiated." -ForegroundColor Green
