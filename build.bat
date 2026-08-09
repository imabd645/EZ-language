@echo off
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
    -march=native ^
    -funroll-loops ^
    -fasynchronous-unwind-tables ^
    -o ez.exe ^
    -I C:\msys64\mingw64\include ^
    -I src ^
    src/main.cpp ^
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
    src/vm/BytecodeVM.cpp ^
    src/vm/BytecodeVM_Execute.cpp ^
    src/vm/BytecodeVM_Objects.cpp ^
    -static -static-libgcc -static-libstdc++ ^
    -DCURL_STATICLIB ^
    -lsqlite3 -lffi ^
    -Wl,-Bstatic -lcurl -lssh2 -lnghttp2 -lngtcp2 -lngtcp2_crypto_ossl -lnghttp3 -lbrotlidec -lbrotlicommon -lzstd -lidn2 -lpsl -liconv -lunistring -lssl -lcrypto -lz ^
    -Wl,-Bdynamic -lws2_32 -lwldap32 -lbcrypt -lcrypt32 -lsecur32 -liphlpapi ^
    -ldwmapi -luxtheme -lgdi32 -luser32 -lcomdlg32 -lcomctl32 -lole32 ^
    -lpthread ^
    -Wl,--subsystem,console
if %errorlevel% neq 0 exit /b %errorlevel%

