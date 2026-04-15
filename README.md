# Ocean Cleanup System

**EcoRoute Solutions** is a C++ coursework project that simulates route planning for marine waste collection across a fixed Indian Ocean cleanup sector. The application combines an isometric visual map, animated route playback, algorithm comparison, and exportable summary reports.

## Overview

The system models a central command depot and a set of offshore cleanup locations. Each simulation day assigns new waste levels to the non-HQ nodes, then compares multiple routing strategies against the same scenario.

Key features:

- Fixed map of marine cleanup waypoints around a central command anchorage
- Daily waste generation with reproducible seed support
- Four route-planning algorithms: Regular, Greedy, MST, and TSP
- Animated route playback on an isometric map
- Side-by-side algorithm comparison with cost metrics
- TXT and CSV export for summaries and comparison results

## Architectural Layout

The refactored codebase is organised by responsibility:

```text
include/
  algorithms/     RouteAlgorithm hierarchy and ComparisonManager
  app/            Top-level application lifecycle and orchestration
  core/           Simulation state, graph model, costs, nodes, event log
  persistence/    Export and reporting helpers
  visualization/  Rendering, animation, dashboard UI, styling

src/
  algorithms/     Routing algorithm implementations
  app/            Application loop and subsystem coordination
  core/           Domain and simulation logic
  persistence/    TXT / CSV output generation
  visualization/  OpenGL map rendering and Dear ImGui dashboard
```

### Separation of Concerns

- `Application` coordinates the window, render loop, UI actions, and mission state.
- `WasteSystem` owns the fixed map, current-day waste levels, threshold logic, and event log.
- `ComparisonManager` runs algorithms through one shared comparison workflow.
- `DashboardUI` renders the control panels, while `DashboardStyle` owns reusable layout and theme rules.
- `FileExporter` handles file output only; `ResultLogger` sits one layer above it for application-facing export actions.

## Algorithms

| Algorithm | Role in the coursework |
|-----------|------------------------|
| Regular   | Baseline route using the default visit order |
| Greedy    | Nearest-neighbour heuristic for local cost reduction |
| MST       | Tree-based route approximation |
| TSP       | Exact bitmask dynamic-programming solver for small node counts |

## Build Requirements

- C++17 compiler (MinGW-w64, MSVC, GCC, or Clang)
- CMake 3.16 or newer
- Git (used by CMake to fetch GLFW the first time you configure)
- OpenGL drivers (already installed on any modern system)

Everything else ships with the repository:

- Dear ImGui sources and backends in [external/imgui/](external/imgui/)
- GLAD OpenGL loader in [external/glad/](external/glad/)
- GLFW is **downloaded and built automatically** the first time CMake configures — no vcpkg, no system install needed.

## Build & Run — one-time setup

After cloning the repository, open a terminal in the project folder and run:

```powershell
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

The first `cmake ..` takes a little longer because it downloads GLFW 3.3.9 from GitHub and builds it as part of the project. All following builds are incremental.

Then launch the app from the build directory:

```powershell
.\SmartWasteClearance.exe
```

Or, on MinGW/Makefile generators, the executable is directly in `build/`:

```bash
./SmartWasteClearance
```

### Choosing a CMake generator

If CMake picks an unexpected generator, you can be explicit:

```powershell
# MinGW (MSYS2)
cmake .. -G "MinGW Makefiles"

# Visual Studio 2022
cmake .. -G "Visual Studio 17 2022"

# Linux / macOS
cmake .. -G "Unix Makefiles"
```

## Using the Simulation

1. Generate a new day to produce waste levels for all non-HQ nodes.
2. Choose a routing algorithm or compare all algorithms.
3. Play or replay the animated route.
4. Review metrics, mission route order, and event log entries.
5. Export the current route summary or the full comparison table.

## Refactor Highlights

The current codebase has been reorganised to improve maintainability without changing the coursework scope:

- Dashboard styling and layout rules were extracted from the main UI implementation.
- Mission-session control in the application layer was split into focused helper methods.
- Algorithm comparison now follows one shared execution path for clearer, fairer orchestration.
- Fixed map data and daily waste generation logic were separated inside `WasteSystem`.
- Export code now uses clearer helper functions and consistent project terminology.

## Output

Runtime exports are written to:

```text
output/
```

Generated files include:

- `summary_*.txt`
- `comparison_*.csv`
- `route_detail_*.txt`
