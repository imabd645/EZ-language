@echo off
C:\msys64\mingw64\bin\g++.exe ^
    -O3 ^
    -march=native ^
    -flto ^
    -funroll-loops ^
    -fomit-frame-pointer ^
    -o ez.exe ^
    -I C:\msys64\mingw64\include ^
    src/main.cpp ^
    src/Lexer.cpp ^
    src/Parser.cpp ^
    src/TypeChecker.cpp ^
    src/Bytecode.cpp ^
    src/BytecodeCompiler.cpp ^
    src/BytecodeVM.cpp ^
    src/Builtins.cpp ^
    src/GUIBuiltins.cpp ^
    src/Runtime.cpp ^
    src/CycleCollector.cpp ^
    src/runtime/Builtins_IO.cpp ^
    src/runtime/Builtins_Math.cpp ^
    src/runtime/Builtins_Net.cpp ^
    src/runtime/Builtins_String.cpp ^
    src/runtime/Builtins_Data.cpp ^
    src/runtime/Builtins_Sys.cpp ^
    src/runtime/Builtins_Buffer.cpp ^
    src/runtime/Builtins_Concurrency.cpp ^
    src/runtime/Builtins_Http.cpp ^
    src/runtime/EventLoop.cpp ^
    -static -static-libgcc -static-libstdc++ ^
    -DCURL_STATICLIB ^
    -lsqlite3 ^
    -Wl,-Bstatic -lcurl -lssh2 -lnghttp2 -lngtcp2 -lngtcp2_crypto_ossl -lnghttp3 -lbrotlidec -lbrotlicommon -lzstd -lidn2 -lpsl -liconv -lunistring -lssl -lcrypto -lz ^
    -Wl,-Bdynamic -lws2_32 -lwldap32 -lbcrypt -lcrypt32 -lsecur32 -liphlpapi ^
    -ldwmapi -luxtheme -lgdi32 -luser32 -lcomdlg32 -lcomctl32 -lole32 ^
    -lpthread ^
    -Wl,--subsystem,console
if %errorlevel% neq 0 exit /b %errorlevel%
.\ez.exe test_orm.ez
