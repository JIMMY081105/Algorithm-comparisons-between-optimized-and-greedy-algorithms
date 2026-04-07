#!/bin/bash
# Smart Waste Clearance System - Dependency Setup Script
# Run this ONCE before building the project.

echo "============================================"
echo " Setting up external dependencies..."
echo "============================================"

mkdir -p external/imgui
mkdir -p external/glad/include/glad
mkdir -p external/glad/include/KHR
mkdir -p external/glad/src

echo ""
echo "[Step 1] Downloading Dear ImGui v1.90.4..."

IMGUI_BASE="https://raw.githubusercontent.com/ocornut/imgui/v1.90.4"

for f in imgui.h imgui.cpp imgui_internal.h imgui_draw.cpp imgui_tables.cpp \
         imgui_widgets.cpp imgui_demo.cpp imconfig.h imstb_rectpack.h \
         imstb_textedit.h imstb_truetype.h; do
    curl -sL "${IMGUI_BASE}/${f}" -o "external/imgui/${f}"
done

for f in imgui_impl_glfw.h imgui_impl_glfw.cpp imgui_impl_opengl3.h \
         imgui_impl_opengl3.cpp imgui_impl_opengl3_loader.h; do
    curl -sL "${IMGUI_BASE}/backends/${f}" -o "external/imgui/${f}"
done

echo "ImGui downloaded."

echo ""
echo "[Step 2] Downloading GLAD (OpenGL 3.3 Compatibility)..."

curl -sL "https://raw.githubusercontent.com/Dav1dde/glad/v0.1.36/include/glad/glad.h" \
    -o "external/glad/include/glad/glad.h"
curl -sL "https://raw.githubusercontent.com/Dav1dde/glad/v0.1.36/include/KHR/khrplatform.h" \
    -o "external/glad/include/KHR/khrplatform.h"
curl -sL "https://raw.githubusercontent.com/Dav1dde/glad/v0.1.36/src/glad.c" \
    -o "external/glad/src/glad.c"

echo "GLAD downloaded."

echo ""
echo "[Step 3] Install GLFW3 via your package manager:"
echo "  Ubuntu/Debian: sudo apt install libglfw3-dev"
echo "  macOS:         brew install glfw"
echo "  vcpkg:         vcpkg install glfw3"
echo ""
echo "============================================"
echo " Setup complete! Build with:"
echo "   mkdir build && cd build"
echo "   cmake .."
echo "   cmake --build ."
echo "============================================"
