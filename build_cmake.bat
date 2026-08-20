@echo off
REM Add MinGW to the PATH temporarily for this script
set PATH=C:\msys64\mingw64\bin;%PATH%

if not exist build mkdir build
cd build

REM Configure CMake
cmake -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release ..

if %errorlevel% neq 0 (
    echo CMake configuration failed.
    cd ..
    exit /b %errorlevel%
)

REM Build using all available CPU cores via CMake's unified build wrapper
cmake --build . -j8

if %errorlevel% neq 0 (
    echo Build failed.
    cd ..
    exit /b %errorlevel%
)

cd ..
echo Build complete! You can run .\build\ez.exe
