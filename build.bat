@echo off
REM =======================================================================
REM  c_web build script (Windows / MSVC + vcpkg)
REM  Requirement: run inside "x64 Native Tools Command Prompt for VS 2022"
REM               and set environment variable VCPKG_ROOT to your vcpkg dir
REM =======================================================================

setlocal

if "%VCPKG_ROOT%"=="" (
    echo [ERROR] VCPKG_ROOT is not set. Please set it to your vcpkg install dir.
    exit /b 1
)

where cl >nul 2>nul
if errorlevel 1 (
    echo [ERROR] cl.exe not found. Please run in "x64 Native Tools Command Prompt for VS 2022".
    exit /b 1
)

echo [1/3] Installing dependencies via vcpkg manifest...
"%VCPKG_ROOT%\vcpkg.exe" install --triplet x64-windows
if errorlevel 1 exit /b 1

echo [2/3] Configuring CMake...
cmake -B build -S . -A x64 ^
    -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake ^
    -DVCPKG_TARGET_TRIPLET=x64-windows
if errorlevel 1 exit /b 1

echo [3/3] Building Release...
cmake --build build --config Release -j
if errorlevel 1 exit /b 1

echo.
echo [DONE] Output: build\Release\c_web.exe
endlocal
