#ifndef APPLICATION_H
#define APPLICATION_H

#include "core/WasteSystem.h"
#include "algorithms/ComparisonManager.h"
#include "visualization/IsometricRenderer.h"
#include "visualization/AnimationController.h"
#include "visualization/DashboardUI.h"
#include "persistence/ResultLogger.h"

struct GLFWwindow;

// The top-level application class that owns all subsystems and runs
// the main loop. This is where initialization, input handling, updating,
// and rendering are coordinated.
//
// Lifecycle:
//   1. init() — create window, set up OpenGL, initialize subsystems
//   2. run()  — main loop: poll events, update state, render frame
//   3. shutdown() — clean up resources
class Application {
private:
    // Window
    GLFWwindow* window;
    int windowWidth;
    int windowHeight;

    // Subsystems
    WasteSystem wasteSystem;
    ComparisonManager comparisonManager;
    IsometricRenderer renderer;
    AnimationController animController;
    DashboardUI dashboardUI;
    ResultLogger resultLogger;

    // Current state
    RouteResult currentResult;
    float lastFrameTime;
    bool initialized;

    // Internal setup helpers
    bool initWindow();
    bool initOpenGL();
    void initImGui();
    void initSimulation();

    // Per-frame processing
    void update(float deltaTime);
    void renderFrame();

    // Handle UI action events from the dashboard
    void handleUIActions(const DashboardUI::UIActions& actions);

    // ImGui frame management
    void beginImGuiFrame();
    void endImGuiFrame();

public:
    Application();
    ~Application();

    // Prevent copying — only one Application instance should exist
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    // Main lifecycle
    bool init();
    void run();
    void shutdown();
};

#endif // APPLICATION_H
