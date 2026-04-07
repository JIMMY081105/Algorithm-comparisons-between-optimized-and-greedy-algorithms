@echo off
REM Smart Waste Clearance System - Dependency Setup Script
REM Run this ONCE before building the project.

echo ============================================
echo  Setting up external dependencies...
echo ============================================

REM Create directories
if not exist "external\imgui" mkdir "external\imgui"
if not exist "external\glad\include\glad" mkdir "external\glad\include\glad"
if not exist "external\glad\include\KHR" mkdir "external\glad\include\KHR"
if not exist "external\glad\src" mkdir "external\glad\src"

echo.
echo [Step 1] Downloading Dear ImGui v1.90.4...
echo.

REM Download ImGui core files from the official repository
set IMGUI_BASE=https://raw.githubusercontent.com/ocornut/imgui/v1.90.4

curl -sL "%IMGUI_BASE%/imgui.h" -o "external/imgui/imgui.h"
curl -sL "%IMGUI_BASE%/imgui.cpp" -o "external/imgui/imgui.cpp"
curl -sL "%IMGUI_BASE%/imgui_internal.h" -o "external/imgui/imgui_internal.h"
curl -sL "%IMGUI_BASE%/imgui_draw.cpp" -o "external/imgui/imgui_draw.cpp"
curl -sL "%IMGUI_BASE%/imgui_tables.cpp" -o "external/imgui/imgui_tables.cpp"
curl -sL "%IMGUI_BASE%/imgui_widgets.cpp" -o "external/imgui/imgui_widgets.cpp"
curl -sL "%IMGUI_BASE%/imgui_demo.cpp" -o "external/imgui/imgui_demo.cpp"
curl -sL "%IMGUI_BASE%/imconfig.h" -o "external/imgui/imconfig.h"
curl -sL "%IMGUI_BASE%/imstb_rectpack.h" -o "external/imgui/imstb_rectpack.h"
curl -sL "%IMGUI_BASE%/imstb_textedit.h" -o "external/imgui/imstb_textedit.h"
curl -sL "%IMGUI_BASE%/imstb_truetype.h" -o "external/imgui/imstb_truetype.h"

REM Download ImGui backend files for GLFW + OpenGL3
curl -sL "%IMGUI_BASE%/backends/imgui_impl_glfw.h" -o "external/imgui/imgui_impl_glfw.h"
curl -sL "%IMGUI_BASE%/backends/imgui_impl_glfw.cpp" -o "external/imgui/imgui_impl_glfw.cpp"
curl -sL "%IMGUI_BASE%/backends/imgui_impl_opengl3.h" -o "external/imgui/imgui_impl_opengl3.h"
curl -sL "%IMGUI_BASE%/backends/imgui_impl_opengl3.cpp" -o "external/imgui/imgui_impl_opengl3.cpp"
curl -sL "%IMGUI_BASE%/backends/imgui_impl_opengl3_loader.h" -o "external/imgui/imgui_impl_opengl3_loader.h"

echo ImGui downloaded successfully.

echo.
echo [Step 2] Downloading GLAD (OpenGL 3.3 Compatibility)...
echo.

REM Download GLAD from a known good source
REM Using the webservice: https://glad.davemorris.io/#language=c&specification=gl&api=gl%3D3.3&api=gles1%3Dnone&api=gles2%3Dnone&api=glsc2%3Dnone&profile=compatibility&loader=on
set GLAD_BASE=https://raw.githubusercontent.com/Dav1dde/glad/glad2/include

REM For simplicity, we use a pre-built glad for GL 3.3 compat
REM If the download fails, generate manually at https://glad.davemorris.io/
curl -sL "https://raw.githubusercontent.com/Dav1dde/glad/v0.1.36/include/glad/glad.h" -o "external/glad/include/glad/glad.h" 2>nul
curl -sL "https://raw.githubusercontent.com/Dav1dde/glad/v0.1.36/include/KHR/khrplatform.h" -o "external/glad/include/KHR/khrplatform.h" 2>nul
curl -sL "https://raw.githubusercontent.com/Dav1dde/glad/v0.1.36/src/glad.c" -o "external/glad/src/glad.c" 2>nul

REM Check if files were downloaded successfully
if not exist "external\glad\include\glad\glad.h" (
    echo.
    echo WARNING: GLAD automatic download may have failed.
    echo Please generate GLAD manually:
    echo   1. Go to https://glad.davemorris.io/
    echo   2. Select: Language=C/C++, API gl=3.3, Profile=Compatibility, Loader=Yes
    echo   3. Click Generate and download
    echo   4. Place glad.h in external/glad/include/glad/
    echo   5. Place khrplatform.h in external/glad/include/KHR/
    echo   6. Place glad.c in external/glad/src/
    echo.
) else (
    echo GLAD downloaded successfully.
)

echo.
echo [Step 3] GLFW Setup
echo.
echo GLFW must be installed separately. Options:
echo   A) Using vcpkg (recommended):
echo      vcpkg install glfw3:x64-windows
echo      Then configure CMake with: -DCMAKE_TOOLCHAIN_FILE=[vcpkg root]/scripts/buildsystems/vcpkg.cmake
echo.
echo   B) Manual installation:
echo      1. Download from https://www.glfw.org/download
echo      2. Extract to a known location
echo      3. Set GLFW3_DIR environment variable or pass -DGLFW3_DIR=path to CMake
echo.
echo ============================================
echo  Setup complete! Now build with CMake:
echo    mkdir build
echo    cd build
echo    cmake ..
echo    cmake --build . --config Release
echo ============================================

pause
