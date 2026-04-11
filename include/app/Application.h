#ifndef APPLICATION_H
#define APPLICATION_H

#include "algorithms/ComparisonManager.h"
#include "core/WasteSystem.h"
#include "environment/EnvironmentController.h"
#include "persistence/ResultLogger.h"
#include "visualization/AnimationController.h"
#include "visualization/DashboardUI.h"
#include "visualization/IsometricRenderer.h"

#include <exception>
#include <string>

struct GLFWwindow;

// Top-level coordinator for the coursework application.
// Application owns the major subsystems and translates UI actions into changes
// to simulation state, playback state, and exported results.
class Application {
private:
    GLFWwindow* window;
    int windowWidth;
    int windowHeight;

    WasteSystem wasteSystem;
    ComparisonManager comparisonManager;
    IsometricRenderer renderer;
    AnimationController animController;
    DashboardUI dashboardUI;
    ResultLogger resultLogger;
    EnvironmentController environmentController;

    RouteResult currentResult;
    MissionPresentation currentMission;
    float lastFrameTime;
    bool initialized;

    bool initWindow();
    bool initOpenGL();
    void initImGui();
    void initSimulation();

    void update(float deltaTime);
    void renderFrame();
    void handleZoomInput();
    void handleCollectedNode(int collectedNodeId);

    // Mission/session helpers keep UI event handling declarative.
    void resetMissionSession();
    void loadMissionRoute(const RouteResult& result, bool autoPlay);
    void replayCurrentMission();
    void logRuntimeError(const std::string& context, const std::exception& e);
    void handleUIActions(const DashboardUI::UIActions& actions);

    void beginImGuiFrame();
    void endImGuiFrame();

public:
    Application();
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    bool init();
    void run();
    void shutdown();
};

#endif // APPLICATION_H
