@echo off
setlocal

REM Compatibility wrapper around the canonical CMake build.
REM The source list and linker configuration live in CMakeLists.txt so the
REM direct and CMake entry points cannot drift apart.

set "ROOT=%~dp0"
if "%ROOT:~-1%"=="\" set "ROOT=%ROOT:~0,-1%"
if "%EZ_ARCH%"=="" set "EZ_ARCH=x86-64-v2"
if "%MSYS2_PREFIX%"=="" set "MSYS2_PREFIX=C:/msys64/mingw64"

where cmake >nul 2>nul
if errorlevel 1 (
    echo CMake was not found on PATH.
    exit /b 1
)

if not exist "%ROOT%\build" mkdir "%ROOT%\build"

cmake -S "%ROOT%" -B "%ROOT%\build" -G "MinGW Makefiles" ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DEZ_ARCH=%EZ_ARCH% ^
    -DMSYS2_PREFIX="%MSYS2_PREFIX%"
if errorlevel 1 (
    echo CMake configuration failed.
    exit /b %errorlevel%
)

cmake --build "%ROOT%\build" --parallel
if errorlevel 1 (
    echo Build failed.
    exit /b %errorlevel%
)

echo Build complete: %ROOT%\build\ez.exe
endlocal
