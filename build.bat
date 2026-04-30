@echo off
REM =======================================================================
REM  drogon-admin build script (Windows / MSVC + vcpkg)
REM  Requirement: run inside "x64 Native Tools Command Prompt for VS 2022"
REM  VCPKG_ROOT: default to E:\dev\vcpkg if not set in environment
REM =======================================================================

setlocal

REM ---------------------------------------------------------------------
REM  VS 2022 Developer Prompt auto-sets VCPKG_ROOT to its bundled vcpkg
REM  (not a git clone). We force-prefer E:\dev\vcpkg when it exists.
REM ---------------------------------------------------------------------
if exist "E:\dev\vcpkg\vcpkg.exe" (
    set "VCPKG_ROOT=E:\dev\vcpkg"
    echo [INFO] Using vcpkg at E:\dev\vcpkg
) else (
    if "%VCPKG_ROOT%"=="" (
        echo [ERROR] E:\dev\vcpkg not found and VCPKG_ROOT not set.
        exit /b 1
    )
    echo [INFO] Using vcpkg at %VCPKG_ROOT%
)

if not exist "%VCPKG_ROOT%\vcpkg.exe" (
    echo [ERROR] vcpkg.exe not found at %VCPKG_ROOT%\vcpkg.exe
    echo         Run bootstrap-vcpkg.bat inside %VCPKG_ROOT% first.
    exit /b 1
)

if not exist "%VCPKG_ROOT%\.git" (
    echo [WARN] %VCPKG_ROOT% is not a git clone; builtin-baseline cannot be resolved.
    echo        Recommended: git clone https://github.com/microsoft/vcpkg.git E:\dev\vcpkg
)

where cl >nul 2>nul
if errorlevel 1 (
    echo [ERROR] cl.exe not found. Please run in "x64 Native Tools Command Prompt for VS 2022".
    exit /b 1
)

echo [1/2] Ensuring vcpkg manifest baseline...

REM vcpkg manifest mode requires a builtin-baseline. Auto-inject if missing.
findstr /C:"builtin-baseline" vcpkg.json >nul 2>nul
if errorlevel 1 (
    echo [INFO] Adding initial builtin-baseline to vcpkg.json ...
    "%VCPKG_ROOT%\vcpkg.exe" --x-manifest-root=. x-update-baseline --add-initial-baseline
    if errorlevel 1 (
        echo [ERROR] x-update-baseline failed. Ensure %VCPKG_ROOT% is a git clone and 'git' is in PATH.
        exit /b 1
    )
)

echo [2/2] Configuring and building (vcpkg manifest mode, overlay triplets)...

REM ---------------------------------------------------------------------
REM  Pin MSVC toolset to the LATEST version installed, because vcpkg uses
REM  vswhere to pick the newest MSVC when building deps (e.g. 14.44), while
REM  the plain vcvars64.bat default toolset can be older (e.g. 14.38). A
REM  mismatch causes LNK2019 on new STL symbols (__std_find_last_trivial_1,
REM  __std_search_1, __std_find_end_1, _Cnd_timedwait_for_unchecked, ...).
REM ---------------------------------------------------------------------
set "LATEST_VCTOOLS=%VCToolsVersion%"
if defined VCINSTALLDIR (
    for /f "delims=" %%v in ('dir /b /ad /on "%VCINSTALLDIR%Tools\MSVC" 2^>nul') do set "LATEST_VCTOOLS=%%v"
)
set "TOOLSET_ARG="
if defined LATEST_VCTOOLS (
    set "TOOLSET_ARG=-T version=%LATEST_VCTOOLS%"
    echo [INFO] Using MSVC toolset version %LATEST_VCTOOLS%
)

cmake -B build -S . -A x64 %TOOLSET_ARG% ^
    -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake ^
    -DVCPKG_TARGET_TRIPLET=x64-windows-static-md ^
    -DVCPKG_OVERLAY_TRIPLETS=%CD%\triplets
if errorlevel 1 exit /b 1

cmake --build build --config Release -j
if errorlevel 1 exit /b 1

echo.
echo [DONE] Output: build\Release\drogon-admin.exe
endlocal
