# SmartWasteClearance

SmartWasteClearance is a C++17 coursework application for planning and comparing waste-collection routes across a fixed cleanup map. It combines backend route optimisation, daily simulation data, an OpenGL/ImGui dashboard, animated route playback, optional AI route advice, and exportable reports.

## One-Line Run

Open a terminal in this project folder and run one command.

### Windows

```bat
run.bat
```

### Linux or macOS

```bash
./run.sh
```

The run script configures CMake, downloads/builds GLFW automatically through CMake, builds the Release executable, and launches the application. No manual Visual Studio project setup, vcpkg setup, or separate GLFW installation is required.

## Required Tools

The machine running the project must have:

- CMake 3.16 or newer
- A C++17 compiler
  - Windows: MinGW-w64, MSVC Build Tools, or Visual Studio
  - Linux: GCC or Clang
  - macOS: Apple Clang/Xcode command line tools
- OpenGL-capable graphics drivers
- Internet access on the first configure step so CMake can download GLFW 3.3.9

The project already includes the required Dear ImGui and GLAD source files under `external/`. If those files are accidentally deleted, run:

```bat
setup_dependencies.bat
```

or on Linux/macOS:

```bash
./setup_dependencies.sh
```

Then run the normal one-line command again.

## What the Application Does

The system simulates a smart waste-clearance operation. A headquarters node acts as the route depot, and surrounding nodes represent collection locations. Each generated day assigns new waste levels, then the dashboard allows the user to run one routing algorithm or compare all available algorithms under the same scenario.

Main capabilities:

- Fixed cleanup map with a central HQ/depot and multiple waste nodes
- Daily scenario generation with repeatable simulation behaviour
- Waste-level thresholds for deciding which nodes are eligible for collection
- Route planning through seven algorithms
- Cost calculation including route distance, collection amount, travel time, fuel usage, tolls, and total cost
- Animated route playback in an isometric OpenGL dashboard
- Event log for simulation actions
- TXT and CSV export for summaries, route details, and algorithm comparisons
- Optional AI chatbot panel for route discussion and suggestions

## Routing Algorithms

| Algorithm | Purpose |
|-----------|---------|
| Regular | Baseline route using the default eligible-node order |
| Greedy | Nearest-neighbour heuristic based on current position |
| MST | Minimum-spanning-tree-inspired coverage route |
| TSP | Exact dynamic-programming solution for small eligible-node sets |
| Dijkstra | Shortest-path ordering based on weighted graph distances |
| Bellman-Ford | Relaxation-based shortest-path comparison strategy |
| Floyd-Warshall | All-pairs shortest-path comparison strategy |

Each algorithm returns a route result that is passed through shared cost-population and comparison logic so the dashboard can compare metrics consistently.

## Project Structure

```text
include/
  ai/              Chatbot service interface
  algorithms/      RouteAlgorithm hierarchy and algorithm headers
  app/             Application lifecycle and top-level orchestration
  core/            Waste nodes, trucks, graph, costs, events, system state
  environment/     Season and mission presentation helpers
  persistence/     Export and result logging interfaces
  visualization/   Renderer, dashboard, style, animation, theme headers

src/
  ai/              Chatbot service implementation
  algorithms/      Route algorithm implementations
  app/             Application loop, actions, and chatbot integration
  core/            Simulation and domain implementation
  environment/     Environment and mission presentation implementation
  persistence/     TXT/CSV export implementation
  visualization/   OpenGL, ImGui, city/sea theme rendering, animation

tests/
  backend/         Backend unit and integration tests
  EnvironmentCoreTests.cpp

external/
  glad/            Vendored OpenGL loader source
  imgui/           Vendored Dear ImGui source and GLFW/OpenGL backends
```

## Build Details

The build is managed by `CMakeLists.txt`.

Important implementation details:

- C++ standard: C++17
- GUI: Dear ImGui
- Window/input layer: GLFW 3.3.9, fetched by CMake from the official GitHub release archive
- OpenGL loader: GLAD, vendored in `external/glad`
- Rendering backend: OpenGL 3
- Windows networking for the optional chatbot: WinINet

The generated executable is named:

```text
SmartWasteClearance
```

On Windows it is:

```text
SmartWasteClearance.exe
```

The one-line runner searches both common CMake output layouts:

- `build/SmartWasteClearance.exe`
- `build/Release/SmartWasteClearance.exe`

## Manual Build Commands

The one-line runner is preferred, but the project can also be built manually.

```bash
cmake -S . -B build
cmake --build build --config Release
```

Run the executable from the build directory after compilation.

## Running Tests

After building, run:

```bash
ctest --test-dir build --output-on-failure
```

Current test targets:

- `EnvironmentCoreTests`
- `BackendLogicTests`

The backend tests cover route algorithms, comparison management, graph behaviour, core simulation logic, persistence exports, and full backend pipeline scenarios.

## Using the Dashboard

Typical workflow:

1. Generate a new day/scenario.
2. Set or review the waste eligibility threshold.
3. Select one algorithm or run all algorithms for comparison.
4. Play or replay the route animation.
5. Inspect distance, time, cost, fuel, toll, and waste-collected metrics.
6. Export the current route summary, route details, or comparison table.

Exports are written to:

```text
output/
```

Generated output examples:

- `summary_*.txt`
- `route_detail_*.txt`
- `comparison_*.csv`

## Optional AI Assistant

The dashboard can optionally use an API key from:

```text
ai_key.txt
```

This file is intentionally ignored by Git because it may contain a private key. The project still builds and runs without it. On Windows, live AI requests use WinINet. Suggested AI routes are validated before use: they must start at HQ, end at HQ, and cover the currently eligible nodes exactly once.

## Troubleshooting

If `run.bat` says CMake is missing, install CMake and make sure it is available on `PATH`.

If CMake cannot find a compiler, install one of:

- Visual Studio with C++ desktop development tools
- Microsoft Build Tools for C++
- MinGW-w64/MSYS2
- GCC or Clang on Linux
- Xcode command line tools on macOS

If the first configure step fails while downloading GLFW, check internet access and run `run.bat` or `./run.sh` again.

If ImGui or GLAD files are missing from `external/`, restore them with `setup_dependencies.bat` or `./setup_dependencies.sh`.

If the app opens but the window is blank or fails to create an OpenGL context, update the graphics driver and confirm the machine supports OpenGL.

## Notes for Marking

For the simplest marking flow on Windows:

```bat
run.bat
```

For Linux/macOS:

```bash
./run.sh
```

That single command is intended to build and launch the submitted project from a clean checkout, provided the required compiler, CMake, graphics driver, and first-run internet access are available.
