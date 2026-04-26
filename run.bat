@echo off
setlocal EnableExtensions

cd /d "%~dp0"

echo ============================================
echo  SmartWasteClearance - build and run
echo ============================================

where cmake >nul 2>nul
if errorlevel 1 (
    echo ERROR: CMake was not found on PATH.
    echo Install CMake 3.16 or newer, then run this file again.
    pause
    exit /b 1
)

if not exist "external\imgui\imgui.cpp" (
    echo ERROR: Missing Dear ImGui source files in external\imgui.
    echo Run setup_dependencies.bat once, then run this file again.
    pause
    exit /b 1
)

if not exist "external\glad\src\glad.c" (
    echo ERROR: Missing GLAD source files in external\glad.
    echo Run setup_dependencies.bat once, then run this file again.
    pause
    exit /b 1
)

if not exist "build" mkdir "build"

echo.
echo [1/3] Configuring CMake...
cmake -S . -B build
if errorlevel 1 (
    pause
    exit /b 1
)

echo.
echo [2/3] Building Release executable...
cmake --build build --config Release
if errorlevel 1 (
    pause
    exit /b 1
)

set "APP_EXE="
if exist "build\Release\SmartWasteClearance.exe" set "APP_EXE=build\Release\SmartWasteClearance.exe"
if "%APP_EXE%"=="" if exist "build\SmartWasteClearance.exe" set "APP_EXE=build\SmartWasteClearance.exe"

if "%APP_EXE%"=="" (
    echo ERROR: Build completed but SmartWasteClearance.exe was not found.
    pause
    exit /b 1
)

echo.
echo [3/3] Launching %APP_EXE%...
"%APP_EXE%"
exit /b %ERRORLEVEL%
