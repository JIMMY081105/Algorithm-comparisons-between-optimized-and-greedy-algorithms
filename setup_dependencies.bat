@echo off
setlocal EnableExtensions

cd /d "%~dp0"

echo ============================================
echo  SmartWasteClearance dependency restore
echo ============================================
echo.
echo This script restores the vendored source files used by the project.
echo GLFW is downloaded automatically by CMake when run.bat configures the build.
echo.

where curl >nul 2>nul
if errorlevel 1 (
    echo ERROR: curl was not found on PATH.
    echo Install curl or use a recent Windows version that includes it.
    exit /b 1
)

if not exist "external\imgui" mkdir "external\imgui"
if not exist "external\glad\include\glad" mkdir "external\glad\include\glad"
if not exist "external\glad\include\KHR" mkdir "external\glad\include\KHR"
if not exist "external\glad\src" mkdir "external\glad\src"

echo [1/2] Downloading Dear ImGui v1.90.4 source files...
set "IMGUI_BASE=https://raw.githubusercontent.com/ocornut/imgui/v1.90.4"

for %%F in (
    imgui.h
    imgui.cpp
    imgui_internal.h
    imgui_draw.cpp
    imgui_tables.cpp
    imgui_widgets.cpp
    imgui_demo.cpp
    imconfig.h
    imstb_rectpack.h
    imstb_textedit.h
    imstb_truetype.h
) do (
    curl -fL "%IMGUI_BASE%/%%F" -o "external\imgui\%%F" || exit /b 1
)

for %%F in (
    imgui_impl_glfw.h
    imgui_impl_glfw.cpp
    imgui_impl_opengl3.h
    imgui_impl_opengl3.cpp
    imgui_impl_opengl3_loader.h
) do (
    curl -fL "%IMGUI_BASE%/backends/%%F" -o "external\imgui\%%F" || exit /b 1
)

echo.
echo [2/2] Downloading GLAD OpenGL loader source files...
curl -fL "https://raw.githubusercontent.com/Dav1dde/glad/v0.1.36/include/glad/glad.h" -o "external\glad\include\glad\glad.h" || exit /b 1
curl -fL "https://raw.githubusercontent.com/Dav1dde/glad/v0.1.36/include/KHR/khrplatform.h" -o "external\glad\include\KHR\khrplatform.h" || exit /b 1
curl -fL "https://raw.githubusercontent.com/Dav1dde/glad/v0.1.36/src/glad.c" -o "external\glad\src\glad.c" || exit /b 1

echo.
echo Dependency source files are ready.
echo Run the project with:
echo   run.bat
