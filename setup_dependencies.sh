#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"

echo "============================================"
echo " SmartWasteClearance dependency restore"
echo "============================================"
echo
echo "This script restores the vendored source files used by the project."
echo "GLFW is downloaded automatically by CMake when run.sh configures the build."
echo

if ! command -v curl >/dev/null 2>&1; then
    echo "ERROR: curl was not found on PATH."
    exit 1
fi

mkdir -p external/imgui
mkdir -p external/glad/include/glad
mkdir -p external/glad/include/KHR
mkdir -p external/glad/src

echo "[1/2] Downloading Dear ImGui v1.90.4 source files..."
IMGUI_BASE="https://raw.githubusercontent.com/ocornut/imgui/v1.90.4"

for file in \
    imgui.h \
    imgui.cpp \
    imgui_internal.h \
    imgui_draw.cpp \
    imgui_tables.cpp \
    imgui_widgets.cpp \
    imgui_demo.cpp \
    imconfig.h \
    imstb_rectpack.h \
    imstb_textedit.h \
    imstb_truetype.h; do
    curl -fL "${IMGUI_BASE}/${file}" -o "external/imgui/${file}"
done

for file in \
    imgui_impl_glfw.h \
    imgui_impl_glfw.cpp \
    imgui_impl_opengl3.h \
    imgui_impl_opengl3.cpp \
    imgui_impl_opengl3_loader.h; do
    curl -fL "${IMGUI_BASE}/backends/${file}" -o "external/imgui/${file}"
done

echo
echo "[2/2] Downloading GLAD OpenGL loader source files..."
curl -fL "https://raw.githubusercontent.com/Dav1dde/glad/v0.1.36/include/glad/glad.h" \
    -o "external/glad/include/glad/glad.h"
curl -fL "https://raw.githubusercontent.com/Dav1dde/glad/v0.1.36/include/KHR/khrplatform.h" \
    -o "external/glad/include/KHR/khrplatform.h"
curl -fL "https://raw.githubusercontent.com/Dav1dde/glad/v0.1.36/src/glad.c" \
    -o "external/glad/src/glad.c"

echo
echo "Dependency source files are ready."
echo "Run the project with:"
echo "  ./run.sh"
