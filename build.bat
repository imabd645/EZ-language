@echo off
setlocal

REM CPU baseline for the produced binary.
REM
REM This was -march=native, which tunes for the CPU of whatever machine ran
REM the build and emits instructions older CPUs do not have. The result runs
REM fine locally and dies with an illegal-instruction crash on any older
REM machine -- before main(), with no diagnostic. That is invisible until a
REM user reports it, so the shipped default is a baseline instead.
REM
REM x86-64-v2 = SSE3/SSSE3/SSE4.1/SSE4.2/POPCNT: Intel Nehalem (2008) and
REM AMD Bulldozer (2011) onward, i.e. every machine that can run Windows 10
REM or 11. An interpreter's hot path is the dispatch loop, not SIMD, so the
REM cost over -march=native is small.
REM
REM For a local, non-distributed build:  set EZ_ARCH=native ^&^& build.bat
if "%EZ_ARCH%"=="" set EZ_ARCH=x86-64-v2
echo Building for -march=%EZ_ARCH%

REM LINK ORDER MATTERS -- the link line must END in -Wl,-Bstatic.
REM
REM -static -static-libstdc++ appear below, but g++ appends libstdc++ and
REM libwinpthread AFTER everything on this line, so whichever -B mode the line
REM ends in decides how they resolve. Ending on the -Wl,-Bdynamic block used
REM for the Windows system libraries silently defeated -static-libstdc++, and
REM ez.exe imported libstdc++-6.dll and libwinpthread-1.dll -- so every built
REM .exe, including bundled ones, needed those two DLLs beside it to start.
REM Verify after changing anything here:
REM   objdump -p ez.exe | findstr /i "DLL Name"
REM Only Windows system DLLs should be listed.
REM
REM -flto and -fomit-frame-pointer were REMOVED and -fasynchronous-unwind-tables
REM added so C++ exception unwinding is reliable. An EZ runtime error (e.g.
REM indexing nil) inside an FFI callback dispatched through the libuv event loop
REM -- exactly how every ezweb request handler runs -- crashed the process even
REM though the error was caught in EZ: LTO produced private clones of the
REM exception path ('.lto_priv.0' in the backtrace) with broken landing-pad
REM tables, so the throw from runtimeError() unwound past run()'s and
REM callFunction()'s catch handlers to a null landing pad. Do not re-add -flto
REM without confirming that path stays crash-free.
C:\msys64\mingw64\bin\g++.exe ^
    -O3 ^
    -march=%EZ_ARCH% ^
    -funroll-loops ^
    -fasynchronous-unwind-tables ^
    -o ez.exe ^
    -I C:\msys64\mingw64\include ^
    -I src ^
    src/main.cpp ^
    src/testing/TestRunner.cpp ^
    src/builtins/Builtins.cpp ^
    src/builtins/Builtins_Buffer.cpp ^
    src/builtins/Builtins_Concurrency.cpp ^
    src/builtins/Builtins_Console.cpp ^
    src/builtins/Builtins_Core.cpp ^
    src/builtins/Builtins_Data.cpp ^
    src/builtins/FFI/FFI.cpp ^
    src/builtins/FFI/FFI_Support.cpp ^
    src/builtins/FFI/FFI_Memory.cpp ^
    src/builtins/FFI/FFI_Call.cpp ^
    src/builtins/FFI/FFI_Callback.cpp ^
    src/builtins/FFI/FFI_Struct.cpp ^
    src/builtins/Builtins_GC.cpp ^
    src/builtins/Builtins_Http.cpp ^
    src/builtins/Builtins_IO.cpp ^
    src/builtins/Builtins_Math.cpp ^
    src/builtins/Builtins_Net.cpp ^
    src/builtins/Builtins_String.cpp ^
    src/builtins/Builtins_TimeDate.cpp ^
    src/bytecode/Bytecode.cpp ^
    src/bytecode/serializer/BytecodeSerializer.cpp ^
    src/cli/CLI.cpp ^
    src/cli/Packager.cpp ^
    src/cli/REPL.cpp ^
    src/compiler/BytecodeCompiler.cpp ^
    src/compiler/BytecodeCompilerExpr.cpp ^
    src/compiler/BytecodeCompilerStmt.cpp ^
    src/eventloop/EventLoop.cpp ^
    src/gc/CycleCollector.cpp ^
    src/gui/GUI_Core.cpp ^
    src/gui/GUI_Dialogs.cpp ^
    src/gui/GUI_Drawing.cpp ^
    src/gui/GUI_Menu.cpp ^
    src/gui/GUI_Widgets.cpp ^
    src/lexer/Lexer.cpp ^
    src/parser/Parser.cpp ^
    src/parser/ParserExpr.cpp ^
    src/parser/ParserStmt.cpp ^
    src/runtime/Runtime.cpp ^
    src/typechecker/TypeChecker.cpp ^
    src/typechecker/TypeCheckerExpr.cpp ^
    src/typechecker/TypeCheckerStmt.cpp ^
    src/utils/EzLibPath.cpp ^
    src/vm/BytecodeVM.cpp ^
    src/vm/BytecodeVM_Execute.cpp ^
    src/vm/BytecodeVM_Objects.cpp ^
    -static -static-libgcc -static-libstdc++ ^
    -DCURL_STATICLIB ^
    -lsqlite3 -lffi ^
    -Wl,-Bstatic -lcurl -lssh2 -lnghttp2 -lngtcp2 -lngtcp2_crypto_ossl -lnghttp3 -lbrotlidec -lbrotlicommon -lzstd -lidn2 -lpsl -liconv -lunistring -lssl -lcrypto -lz ^
    -Wl,-Bdynamic -lws2_32 -lwldap32 -lbcrypt -lcrypt32 -lsecur32 -liphlpapi ^
    -ldwmapi -luxtheme -lgdi32 -luser32 -lcomdlg32 -lcomctl32 -lole32 ^
    -Wl,-Bstatic -lstdc++ -lwinpthread ^
    -Wl,--subsystem,console
if %errorlevel% neq 0 exit /b %errorlevel%
endlocal

