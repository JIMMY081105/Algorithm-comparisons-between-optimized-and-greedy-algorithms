# Smart Waste Clearance and Management System

**EcoRoute Solutions Sdn Bhd** — A simulation and decision-support tool for smart waste collection route planning in Malaysia.

## Overview

This C++ application simulates waste collection operations for a fictitious Malaysian waste management company. It provides:

- A fixed map of 10 waste collection locations + 1 HQ/depot
- Random waste level generation for each simulation day
- Four routing algorithms (Regular, Greedy, MST, TSP) for comparison
- An isometric 3D-style dashboard with route animation
- Cost analysis: distance, time, fuel, wages, total operating cost
- File export of results in TXT and CSV formats

## Prerequisites

- **C++17** compatible compiler (MSVC 2019+, GCC 9+, Clang 10+)
- **CMake** 3.16 or later
- **GLFW3** (window management)
- **OpenGL** (system-provided)
- **Dear ImGui** (downloaded by setup script)
- **GLAD** (downloaded by setup script)

## Quick Start

### 1. Download Dependencies

Run the setup script to download ImGui and GLAD:

**Windows:**
```
setup_dependencies.bat
```

**Linux/macOS:**
```
chmod +x setup_dependencies.sh
./setup_dependencies.sh
```

### 2. Install GLFW

**Option A — vcpkg (recommended for Windows):**
```
vcpkg install glfw3:x64-windows
```

**Option B — System package manager:**
```
# Ubuntu/Debian
sudo apt install libglfw3-dev

# macOS
brew install glfw
```

### 3. Build

```
mkdir build
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=[vcpkg root]/scripts/buildsystems/vcpkg.cmake
cmake --build . --config Release
```

If not using vcpkg, just run `cmake ..` without the toolchain flag.

### 4. Run

```
./SmartWasteClearance      # Linux/macOS
SmartWasteClearance.exe    # Windows
```

## Project Structure

```
include/
  core/           WasteNode, Truck, MapGraph, CostModel, RouteResult, WasteSystem, EventLog
  algorithms/     RouteAlgorithm (abstract), Regular, Greedy, MST, TSP, ComparisonManager
  visualization/  IsometricRenderer, AnimationController, DashboardUI, RenderUtils
  persistence/    FileExporter, ResultLogger
  app/            Application

src/              Corresponding .cpp implementations
external/         ImGui, GLAD (downloaded by setup script)
output/           Export files generated at runtime
```

## Algorithms

| Algorithm | Approach | Complexity |
|-----------|----------|------------|
| Regular   | Visit eligible nodes in default ID order | O(n) |
| Greedy    | Nearest-neighbor heuristic | O(n^2) |
| MST       | Prim's MST + DFS traversal | O(n^2) |
| TSP       | Exact bitmask DP | O(n^2 * 2^n) |

## How to Use

1. Click **Generate New Day** to randomize waste levels
2. Select an algorithm and click **Run Selected Algorithm**
3. Watch the animated route on the isometric map
4. Use **Compare All Algorithms** to see side-by-side metrics
5. Export results via the Export panel

## C++ Concepts Demonstrated

- Classes and objects (all domain entities)
- Inheritance and polymorphism (RouteAlgorithm hierarchy)
- File I/O with exception handling (FileExporter)
- Vectors and adjacency matrices (MapGraph)
- Manual linked list (EventLog)
- Structures (RouteResult, Color, IsoCoord)
- Smart pointers (ComparisonManager owns algorithms via unique_ptr)
- Random number generation (Mersenne Twister with seed control)
- Modular multi-file architecture
