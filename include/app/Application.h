#ifndef APPLICATION_H
#define APPLICATION_H

#include "core/RouteResult.h"
#include "environment/EnvironmentTypes.h"
#include "environment/MissionPresentation.h"
#include "visualization/DashboardActions.h"

#include <exception>
#include <memory>
#include <string>
#include <vector>

struct GLFWwindow;

class AnimationController;
class ChatbotPanel;
class ChatbotService;
class ComparisonManager;
class DashboardUI;
class EnvironmentController;
class IsometricRenderer;
class ResultLogger;
class WasteSystem;

struct GlfwWindowDeleter {
    void operator()(GLFWwindow* window) const noexcept;
};

// Top-level coordinator for the coursework application.
// Application owns the major subsystems and translates UI actions into changes
// to simulation state, playback state, and exported results.
class Application {
private:
    std::unique_ptr<GLFWwindow, GlfwWindowDeleter> window;
    int windowWidth;
    int windowHeight;

    std::unique_ptr<WasteSystem> wasteSystem;
    std::unique_ptr<ComparisonManager> comparisonManager;
    std::unique_ptr<IsometricRenderer> renderer;
    std::unique_ptr<AnimationController> animController;
    std::unique_ptr<DashboardUI> dashboardUI;
    std::unique_ptr<ResultLogger> resultLogger;
    std::unique_ptr<EnvironmentController> environmentController;
    std::unique_ptr<ChatbotService> chatbotService;
    std::unique_ptr<ChatbotPanel> chatbotPanel;

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
    void handleThemeChange(EnvironmentTheme theme);
    void handleCitySeasonChange(CitySeason season);
    void refreshCityWeather();
    void generateNewDay();
    void runSelectedAlgorithm(int algorithmIndex);
    void compareAllAlgorithms();
    void playOrRestartMission();
    void exportCurrentResult();
    void exportComparisonResults();

    // Mission/session helpers keep UI event handling declarative.
    void resetMissionSession();
    void refreshSessionAfterWeightChange();
    void loadMissionRoute(const RouteResult& result, bool autoPlay);
    void replayCurrentMission();
    void logRuntimeError(const std::string& context, const std::exception& e);
    void handleUIActions(const DashboardUIActions& actions);

    void beginImGuiFrame();
    void endImGuiFrame();

    // AI assistant helpers — build system context, dispatch user prompts, and
    // convert a recommended visit order into a RouteResult the animation
    // controller can replay like any other algorithm output.
    std::string buildChatbotContext() const;
    void renderChatbotPanel();
    void pollChatbotResponse();
    void applyRecommendedRoute(const std::vector<int>& order);

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
