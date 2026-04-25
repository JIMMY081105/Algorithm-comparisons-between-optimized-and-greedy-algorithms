#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"

echo "============================================"
echo " SmartWasteClearance - build and run"
echo "============================================"

if ! command -v cmake >/dev/null 2>&1; then
    echo "ERROR: CMake was not found on PATH."
    echo "Install CMake 3.16 or newer, then run this script again."
    exit 1
fi

if [ ! -f "external/imgui/imgui.cpp" ]; then
    echo "ERROR: Missing Dear ImGui source files in external/imgui."
    echo "Run ./setup_dependencies.sh once, then run this script again."
    exit 1
fi

if [ ! -f "external/glad/src/glad.c" ]; then
    echo "ERROR: Missing GLAD source files in external/glad."
    echo "Run ./setup_dependencies.sh once, then run this script again."
    exit 1
fi

echo
echo "[1/3] Configuring CMake..."
cmake -S . -B build

echo
echo "[2/3] Building Release executable..."
cmake --build build --config Release

app=""
if [ -x "build/Release/SmartWasteClearance" ]; then
    app="build/Release/SmartWasteClearance"
elif [ -x "build/SmartWasteClearance" ]; then
    app="build/SmartWasteClearance"
fi

if [ -z "$app" ]; then
    echo "ERROR: Build completed but SmartWasteClearance was not found."
    exit 1
fi

echo
echo "[3/3] Launching $app..."
"$app"
