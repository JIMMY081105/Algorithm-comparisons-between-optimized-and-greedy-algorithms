#include "app/Application.h"

#include "ai/ChatbotService.h"
#include "algorithms/ComparisonManager.h"
#include "core/WasteSystem.h"
#include "environment/EnvironmentController.h"
#include "environment/SeasonProfile.h"
#include "persistence/ResultLogger.h"
#include "visualization/AnimationController.h"
#include "visualization/ChatbotPanel.h"
#include "visualization/DashboardUI.h"
#include "visualization/IsometricRenderer.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <cmath>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>

namespace {
constexpr float kMaxFrameDeltaTime = 0.1f;
constexpr unsigned int kWeatherSeedDayMultiplier = 97u;
constexpr unsigned int kTrafficSeedDayMultiplier = 131u;
constexpr double kMillisecondsPerSecond = 1000.0;

float clampFrameDelta(float deltaTime) {
    return (deltaTime > kMaxFrameDeltaTime) ? kMaxFrameDeltaTime : deltaTime;
}

unsigned int buildWeatherSeed(const WasteSystem& system) {
    return system.getCurrentSeed() +
           static_cast<unsigned int>(system.getDayNumber()) *
               kWeatherSeedDayMultiplier +
           static_cast<unsigned int>(glfwGetTime() * kMillisecondsPerSecond);
}

unsigned int buildTrafficSeed(const WasteSystem& system) {
    return system.getCurrentSeed() ^
           (static_cast<unsigned int>(system.getDayNumber()) *
            kTrafficSeedDayMultiplier);
}
} // namespace

// GLFW error callback — logs errors to stderr
static void glfwErrorCallback(int error, const char* description) {
    std::cerr << "GLFW Error " << error << ": " << description << std::endl;
}

void GlfwWindowDeleter::operator()(GLFWwindow* window) const noexcept {
    if (window != nullptr) {
        glfwDestroyWindow(window);
    }
}

Application::Application()
    : window(nullptr),
      windowWidth(1280),
      windowHeight(850),
      wasteSystem(std::make_unique<WasteSystem>()),
      comparisonManager(std::make_unique<ComparisonManager>()),
      renderer(std::make_unique<IsometricRenderer>()),
      animController(std::make_unique<AnimationController>()),
      dashboardUI(std::make_unique<DashboardUI>()),
      resultLogger(std::make_unique<ResultLogger>()),
      environmentController(std::make_unique<EnvironmentController>()),
      chatbotService(std::make_unique<ChatbotService>()),
      chatbotPanel(std::make_unique<ChatbotPanel>()),
      lastFrameTime(0.0f), initialized(false) {}

Application::~Application() {
    shutdown();
}

bool Application::init() {
    try {
        if (!initWindow()) {
            shutdown();
            return false;
        }
        if (!initOpenGL()) {
            shutdown();
            return false;
        }
        initImGui();
        initSimulation();
        renderFrame();
        glfwSwapBuffers(window.get());
        glfwShowWindow(window.get());
        glfwFocusWindow(window.get());
        initialized = true;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Initialization failed: " << e.what() << std::endl;
        shutdown();
        return false;
    }
}

bool Application::initWindow() {
    glfwSetErrorCallback(glfwErrorCallback);

    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return false;
    }

    // Use Compatibility Profile so we can use glBegin/glEnd for
    // simple isometric rendering alongside the OpenGL 3 ImGui backend
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_COMPAT_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

    GLFWmonitor* primaryMonitor = glfwGetPrimaryMonitor();
    if (primaryMonitor != nullptr) {
        int workX = 0;
        int workY = 0;
        int workWidth = 0;
        int workHeight = 0;
        glfwGetMonitorWorkarea(primaryMonitor, &workX, &workY,
                               &workWidth, &workHeight);
        if (workWidth > 0 && workHeight > 0) {
            windowWidth = workWidth;
            windowHeight = workHeight;
        }
    }

    const char* title = "EcoRoute Solutions - Dual Environment Simulation Dashboard";
    window.reset(glfwCreateWindow(windowWidth, windowHeight, title, nullptr, nullptr));
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(window.get());
    glfwSwapInterval(1);  // enable vsync for smooth animation

    return true;
}

bool Application::initOpenGL() {
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return false;
    }

    std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << std::endl;
    std::cout << "GLSL Version: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << std::endl;
    std::cout << "Renderer: " << glGetString(GL_RENDERER) << std::endl;

    // Enable blending for transparency
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Smooth lines for roads and route highlights
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);

    return true;
}

void Application::initImGui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Premium dark ocean theme — translucent panels with teal accents
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 8.0f;
    style.FrameRounding = 5.0f;
    style.GrabRounding = 4.0f;
    style.ScrollbarRounding = 6.0f;
    style.TabRounding = 4.0f;
    style.ChildRounding = 6.0f;
    style.PopupRounding = 6.0f;
    style.FramePadding = ImVec2(10, 5);
    style.ItemSpacing = ImVec2(8, 5);
    style.ItemInnerSpacing = ImVec2(6, 4);
    style.WindowPadding = ImVec2(12, 10);
    style.ScrollbarSize = 11.0f;
    style.GrabMinSize = 10.0f;
    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;
    style.WindowTitleAlign = ImVec2(0.5f, 0.5f);
    style.SeparatorTextBorderSize = 2.0f;

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg]            = ImVec4(0.05f, 0.07f, 0.11f, 0.90f);
    colors[ImGuiCol_ChildBg]             = ImVec4(0.05f, 0.07f, 0.10f, 0.35f);
    colors[ImGuiCol_PopupBg]             = ImVec4(0.05f, 0.07f, 0.11f, 0.95f);
    colors[ImGuiCol_Border]              = ImVec4(0.10f, 0.20f, 0.30f, 0.45f);
    colors[ImGuiCol_TitleBg]             = ImVec4(0.04f, 0.06f, 0.10f, 1.00f);
    colors[ImGuiCol_TitleBgActive]       = ImVec4(0.06f, 0.12f, 0.20f, 1.00f);
    colors[ImGuiCol_Header]              = ImVec4(0.08f, 0.16f, 0.26f, 0.72f);
    colors[ImGuiCol_HeaderHovered]       = ImVec4(0.12f, 0.24f, 0.38f, 0.80f);
    colors[ImGuiCol_HeaderActive]        = ImVec4(0.08f, 0.28f, 0.46f, 0.90f);
    colors[ImGuiCol_Button]              = ImVec4(0.08f, 0.20f, 0.32f, 0.88f);
    colors[ImGuiCol_ButtonHovered]       = ImVec4(0.12f, 0.30f, 0.48f, 1.00f);
    colors[ImGuiCol_ButtonActive]        = ImVec4(0.06f, 0.36f, 0.54f, 1.00f);
    colors[ImGuiCol_FrameBg]             = ImVec4(0.06f, 0.09f, 0.14f, 0.88f);
    colors[ImGuiCol_FrameBgHovered]      = ImVec4(0.10f, 0.15f, 0.22f, 1.00f);
    colors[ImGuiCol_FrameBgActive]       = ImVec4(0.08f, 0.18f, 0.28f, 1.00f);
    colors[ImGuiCol_SliderGrab]          = ImVec4(0.14f, 0.52f, 0.74f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]    = ImVec4(0.18f, 0.60f, 0.84f, 1.00f);
    colors[ImGuiCol_CheckMark]           = ImVec4(0.18f, 0.66f, 0.88f, 1.00f);
    colors[ImGuiCol_Separator]           = ImVec4(0.10f, 0.18f, 0.28f, 0.55f);
    colors[ImGuiCol_SeparatorHovered]    = ImVec4(0.14f, 0.32f, 0.50f, 0.78f);
    colors[ImGuiCol_SeparatorActive]     = ImVec4(0.14f, 0.46f, 0.68f, 1.00f);
    colors[ImGuiCol_Tab]                 = ImVec4(0.06f, 0.12f, 0.20f, 0.88f);
    colors[ImGuiCol_TabHovered]          = ImVec4(0.12f, 0.28f, 0.44f, 1.00f);
    colors[ImGuiCol_TableHeaderBg]       = ImVec4(0.06f, 0.10f, 0.16f, 1.00f);
    colors[ImGuiCol_TableBorderStrong]   = ImVec4(0.10f, 0.18f, 0.28f, 0.65f);
    colors[ImGuiCol_TableBorderLight]    = ImVec4(0.08f, 0.14f, 0.22f, 0.45f);
    colors[ImGuiCol_TableRowBg]          = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt]       = ImVec4(0.06f, 0.08f, 0.12f, 0.35f);
    colors[ImGuiCol_TextSelectedBg]      = ImVec4(0.12f, 0.34f, 0.52f, 0.42f);
    colors[ImGuiCol_ScrollbarBg]         = ImVec4(0.03f, 0.05f, 0.08f, 0.55f);
    colors[ImGuiCol_ScrollbarGrab]       = ImVec4(0.12f, 0.20f, 0.30f, 0.76f);
    colors[ImGuiCol_ScrollbarGrabHovered]= ImVec4(0.16f, 0.26f, 0.38f, 0.88f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.18f, 0.32f, 0.48f, 1.00f);
    colors[ImGuiCol_PlotHistogram]       = ImVec4(0.14f, 0.52f, 0.74f, 0.88f);

    ImGui_ImplGlfw_InitForOpenGL(window.get(), true);
    ImGui_ImplOpenGL3_Init("#version 130");
}

void Application::initSimulation() {
    wasteSystem->initializeMap();
    comparisonManager->initializeAlgorithms();
    if (!environmentController->init()) {
        throw std::runtime_error("Failed to initialize environment themes");
    }

    environmentController->rebuildScenes(wasteSystem->getGraph());
    wasteSystem->generateNewDay();
    environmentController->randomizeCityTraffic(buildTrafficSeed(*wasteSystem),
                                               wasteSystem->getGraph());
    environmentController->randomizeCityWeather(buildWeatherSeed(*wasteSystem),
                                               wasteSystem->getGraph());
    environmentController->applyActiveWeights(wasteSystem->getGraph());

    chatbotService->loadKeyFromFile();

    lastFrameTime = static_cast<float>(glfwGetTime());
}

void Application::run() {
    if (!initialized) {
        std::cerr << "Application not initialized" << std::endl;
        return;
    }

    while (!glfwWindowShouldClose(window.get())) {
        glfwPollEvents();

        // Delta time for frame-rate independent animation
        float currentTime = static_cast<float>(glfwGetTime());
        float deltaTime = currentTime - lastFrameTime;
        lastFrameTime = currentTime;

        // Prevent large dt spikes from pauses/breakpoints
        deltaTime = clampFrameDelta(deltaTime);

        update(deltaTime);
        renderFrame();

        glfwSwapBuffers(window.get());
    }
}

void Application::update(float deltaTime) {
    environmentController->update(deltaTime);

    const int collectedNodeId = animController->update(deltaTime);
    if (collectedNodeId >= 0) {
        handleCollectedNode(collectedNodeId);
    }
}

void Application::renderFrame() {
    glfwGetFramebufferSize(window.get(), &windowWidth, &windowHeight);
    glViewport(0, 0, windowWidth, windowHeight);
    beginImGuiFrame();
    handleZoomInput();
    RenderUtils::updateProjection(static_cast<float>(windowWidth),
                                  static_cast<float>(windowHeight),
                                  wasteSystem->getGraph());

    // Clear with dark background
    auto bg = RenderUtils::getBackgroundColor();
    glClearColor(bg.r, bg.g, bg.b, bg.a);
    glClear(GL_COLOR_BUFFER_BIT);

    // Set up 2D orthographic projection for the isometric map
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, windowWidth, windowHeight, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // Render the active environment scene.
    float dt = 1.0f / 60.0f;
    renderer->render(environmentController->activeRenderer(),
                    wasteSystem->getGraph(),
                    animController->getTruck(),
                    currentMission.isValid() ? &currentMission : nullptr,
                    animController->getState(),
                    animController->getProgress(),
                    dt);

    if (environmentController->getTransitionAlpha() > 0.001f) {
        const float alpha = environmentController->getTransitionAlpha();
        glBegin(GL_QUADS);
        glColor4f(0.96f, 0.98f, 1.0f, alpha * 0.08f);
        glVertex2f(0.0f, 0.0f);
        glVertex2f(static_cast<float>(windowWidth), 0.0f);
        glColor4f(0.96f, 0.98f, 1.0f, alpha * 0.14f);
        glVertex2f(static_cast<float>(windowWidth), static_cast<float>(windowHeight));
        glVertex2f(0.0f, static_cast<float>(windowHeight));
        glEnd();
    }

    // Render ImGui dashboard on top
    auto actions = dashboardUI->render(*wasteSystem, *comparisonManager,
                                      *animController, currentResult,
                                      currentMission,
                                      environmentController->getDashboardInfo(),
                                      environmentController->getLayerToggles(),
                                      environmentController->getActiveTheme());

    // Process all UI actions
    handleUIActions(actions);

    renderChatbotPanel();
    pollChatbotResponse();

    endImGuiFrame();
}

void Application::handleZoomInput() {
    ImGuiIO& io = ImGui::GetIO();

    if (!io.WantCaptureMouse && std::abs(io.MouseWheel) > 0.001f) {
        // Standard map behavior: wheel up zooms in, wheel down zooms out.
        RenderUtils::adjustZoom(io.MouseWheel);
    }

    if (io.WantTextInput) return;

    const bool zoomIn =
        ImGui::IsKeyPressed(ImGuiKey_Equal, false) ||
        ImGui::IsKeyPressed(ImGuiKey_KeypadAdd, false);
    const bool zoomOut =
        ImGui::IsKeyPressed(ImGuiKey_Minus, false) ||
        ImGui::IsKeyPressed(ImGuiKey_KeypadSubtract, false);
    const bool resetZoom =
        (io.KeyCtrl || io.KeySuper) && ImGui::IsKeyPressed(ImGuiKey_0, false);

    if (zoomIn) RenderUtils::adjustZoom(1.0f);
    if (zoomOut) RenderUtils::adjustZoom(-1.0f);
    if (resetZoom) RenderUtils::resetZoom();
}

void Application::handleCollectedNode(int collectedNodeId) {
    wasteSystem->markNodeCollected(collectedNodeId);

    const int nodeIndex = wasteSystem->getGraph().findNodeIndex(collectedNodeId);
    if (nodeIndex < 0) {
        return;
    }

    const WasteNode& node = wasteSystem->getGraph().getNode(nodeIndex);
    if (!node.getIsHQ()) {
        wasteSystem->getEventLog().addEvent(
            "Cleaned up garbage at " + node.getName());
    }
}

void Application::handleThemeChange(EnvironmentTheme theme) {
    if (!environmentController->setActiveTheme(theme, wasteSystem->getGraph())) {
        return;
    }

    resetMissionSession();
    wasteSystem->getEventLog().addEvent(
        std::string("Environment switched to ") + toDisplayString(theme));
}

void Application::handleCitySeasonChange(CitySeason season) {
    environmentController->setCitySeason(season, wasteSystem->getGraph());
    resetMissionSession();
    wasteSystem->getEventLog().addEvent(
        std::string("City season set to ") + toDisplayString(season));
}

void Application::refreshCityWeather() {
    if (environmentController->getActiveTheme() != EnvironmentTheme::City) {
        return;
    }

    environmentController->randomizeCityWeather(buildWeatherSeed(*wasteSystem),
                                               wasteSystem->getGraph());
    refreshSessionAfterWeightChange();
    wasteSystem->getEventLog().addEvent("City weather refreshed");
}

void Application::generateNewDay() {
    wasteSystem->generateNewDay();
    environmentController->randomizeCityTraffic(buildTrafficSeed(*wasteSystem),
                                               wasteSystem->getGraph());
    environmentController->randomizeCityWeather(buildWeatherSeed(*wasteSystem),
                                               wasteSystem->getGraph());
    environmentController->applyActiveWeights(wasteSystem->getGraph());
    resetMissionSession();
}

void Application::runSelectedAlgorithm(int algorithmIndex) {
    try {
        loadMissionRoute(
            comparisonManager->runSingleAlgorithm(algorithmIndex, *wasteSystem),
            true);
    } catch (const std::exception& e) {
        logRuntimeError("Error: ", e);
    }
}

void Application::compareAllAlgorithms() {
    try {
        comparisonManager->runAllAlgorithms(*wasteSystem);

        const int bestIndex = comparisonManager->getBestAlgorithmIndex();
        if (bestIndex < 0) {
            return;
        }

        loadMissionRoute(comparisonManager->getResults()[bestIndex], false);
    } catch (const std::exception& e) {
        logRuntimeError("Comparison error: ", e);
    }
}

void Application::playOrRestartMission() {
    if (!currentResult.isValid()) {
        return;
    }

    if (animController->isFinished()) {
        replayCurrentMission();
        return;
    }

    wasteSystem->resetCollectionStatus();
    animController->play();
}

void Application::exportCurrentResult() {
    if (!currentResult.isValid()) {
        return;
    }

    try {
        const std::string filename =
            resultLogger->logCurrentResult(currentResult, *wasteSystem);
        wasteSystem->getEventLog().addEvent("Saved: " + filename);
    } catch (const std::exception& e) {
        logRuntimeError("Export error: ", e);
    }
}

void Application::exportComparisonResults() {
    try {
        const std::string filename =
            resultLogger->logComparison(*comparisonManager, *wasteSystem);
        if (!filename.empty()) {
            wasteSystem->getEventLog().addEvent("Saved: " + filename);
        }
    } catch (const std::exception& e) {
        logRuntimeError("Export error: ", e);
    }
}

void Application::resetMissionSession() {
    currentResult = RouteResult();
    currentMission = MissionPresentation();
    animController->stop();
    renderer->resetAnimation();
    comparisonManager->clearResults();
    wasteSystem->resetCollectionStatus();
}

void Application::refreshSessionAfterWeightChange() {
    if (!comparisonManager->getResults().empty()) {
        compareAllAlgorithms();
    } else {
        resetMissionSession();
    }
}

void Application::loadMissionRoute(const RouteResult& result, bool autoPlay) {
    currentResult = result;
    currentMission = environmentController->buildMissionPresentation(
        currentResult, wasteSystem->getGraph());
    animController->loadRoute(currentMission);
    wasteSystem->resetCollectionStatus();

    if (autoPlay) {
        animController->play();
    }
}

void Application::replayCurrentMission() {
    if (!currentMission.isValid()) {
        return;
    }

    wasteSystem->resetCollectionStatus();
    animController->replay();
}

void Application::logRuntimeError(const std::string& context,
                                  const std::exception& e) {
    wasteSystem->getEventLog().addEvent(context + e.what());
}

void Application::handleUIActions(const DashboardUIActions& actions) {
    if (actions.layerTogglesChanged) {
        environmentController->setLayerToggles(actions.layerToggles);
    }

    if (actions.changeTheme) {
        handleThemeChange(actions.selectedTheme);
    }

    if (actions.changeSeason) {
        handleCitySeasonChange(actions.selectedSeason);
    }

    if (actions.randomizeWeather) {
        refreshCityWeather();
    }

    if (actions.generateNewDay) {
        generateNewDay();
    }

    if (actions.roadEventsChanged) {
        environmentController->applyActiveWeights(wasteSystem->getGraph());
        refreshSessionAfterWeightChange();
    }

    if (actions.runSelectedAlgorithm && actions.algorithmToRun >= 0) {
        runSelectedAlgorithm(actions.algorithmToRun);
    }

    if (actions.compareAll) {
        compareAllAlgorithms();
    }

    if (actions.playPause) {
        playOrRestartMission();
    }

    if (actions.replay) {
        replayCurrentMission();
    }

    if (actions.exportResults) {
        exportCurrentResult();
    }

    if (actions.exportComparison) {
        exportComparisonResults();
    }
}

void Application::beginImGuiFrame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void Application::endImGuiFrame() {
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void Application::shutdown() {
    if (!initialized && window == nullptr && ImGui::GetCurrentContext() == nullptr) {
        return;
    }

    renderer->cleanup();
    environmentController->cleanup();

    if (ImGui::GetCurrentContext() != nullptr) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }

    window.reset();
    glfwTerminate();

    initialized = false;
}

std::string Application::buildChatbotContext() const {
    // Give Gemini a compact, authoritative view of the current day so it can
    // reason about the map without hallucinating coordinates or waste levels.
    std::ostringstream out;
    const ThemeDashboardInfo info = environmentController->getDashboardInfo();
    const MapGraph& graph = wasteSystem->getGraph();

    out << "You are the in-game AI assistant for the EcoRoute Smart Waste "
           "Clearance simulation. Only answer questions about this simulation "
           "or its current state. If asked something unrelated, politely "
           "decline.\n\n";
    out << "CURRENT STATE\n";
    out << "Environment: " << info.themeLabel << " (" << info.subtitle << ")\n";
    if (info.supportsSeasons) {
        out << "Season: " << info.seasonLabel << "\n";
    }
    if (info.supportsWeather) {
        out << "Weather: " << info.weatherLabel << "\n";
    }
    out << "Atmosphere: " << info.atmosphereLabel << "\n";
    out << "Congestion: " << info.congestionLevel
        << "  Incidents: " << info.incidentCount << "\n";
    out << "Day: " << wasteSystem->getDayNumber()
        << "  Seed: " << wasteSystem->getCurrentSeed() << "\n";
    out << "Collection threshold (percent): "
        << wasteSystem->getCollectionThreshold() << "\n";
    out << "HQ node id: " << graph.getHQNode().getId()
        << " (" << graph.getHQNode().getName() << ")\n\n";

    out << "NODES (id | name | x | y | waste%)\n";
    for (int i = 0; i < graph.getNodeCount(); ++i) {
        const WasteNode& node = graph.getNode(i);
        out << node.getId() << " | " << node.getName()
            << " | " << node.getWorldX()
            << " | " << node.getWorldY()
            << " | ";
        if (node.getIsHQ()) {
            out << "HQ";
        } else {
            out << node.getWasteLevel() << "%";
        }
        out << "\n";
    }

    const std::vector<int> eligible = wasteSystem->getEligibleNodes();
    out << "\nELIGIBLE NODE IDS (waste >= threshold): ";
    for (std::size_t i = 0; i < eligible.size(); ++i) {
        if (i > 0) out << ",";
        out << eligible[i];
    }
    out << "\n";

    const auto& results = comparisonManager->getResults();
    if (!results.empty()) {
        out << "\nRECENT ALGORITHM RESULTS\n";
        for (const RouteResult& r : results) {
            if (!r.isValid()) continue;
            out << r.algorithmName
                << ": distance=" << r.totalDistance << "km"
                << ", cost=RM" << r.totalCost
                << ", order=";
            for (std::size_t i = 0; i < r.visitOrder.size(); ++i) {
                if (i > 0) out << ",";
                out << r.visitOrder[i];
            }
            out << "\n";
        }
    }

    out << "\nRESPONSE RULES\n";
    out << "- Keep answers under 150 words.\n";
    out << "- When recommending the best route, the route MUST visit EVERY "
           "node in the ELIGIBLE list above — no node may be skipped. "
           "Optimise the visiting order to minimise total travel distance "
           "while accounting for weather/season conditions.\n";
    out << "- The route MUST start and end at the HQ node id "
        << graph.getHQNode().getId() << ".\n";
    out << "- Only visit nodes from the ELIGIBLE list above, but you MUST "
           "visit ALL of them.\n";
    out << "- Always end your reply with ONE line in this exact format:\n"
           "  ROUTE: id1,id2,id3,...,0\n"
           "  where the list includes ALL eligible node IDs.\n"
           "  If the user did not ask for a route, write:  ROUTE: NONE\n";
    return out.str();
}

void Application::renderChatbotPanel() {
    const std::string context = buildChatbotContext();
    ChatbotPanel::Actions actions = chatbotPanel->render(*chatbotService, context);

    if (actions.submitQuestion && !actions.prompt.empty()) {
        chatbotService->submit(actions.prompt, context, false);
    } else if (actions.requestBestRoute) {
        const std::string prompt =
            "Recommend the optimal pickup route for today. The route MUST "
            "visit ALL eligible nodes to collect all garbage — do not skip "
            "any. Find the best ordering to minimise total travel distance. "
            "Explain briefly why this ordering is good given the weather "
            "and node positions. End with the ROUTE: line containing every "
            "eligible node ID.";
        chatbotService->submit(prompt, context, true);
    }
}

void Application::pollChatbotResponse() {
    ChatbotService::Response response;
    if (!chatbotService->tryConsumeResponse(response)) {
        return;
    }

    chatbotPanel->setLastReply(response.text);

    if (response.isRecommendation && !response.recommendedRoute.empty()) {
        applyRecommendedRoute(response.recommendedRoute);
    } else if (!response.isRecommendation) {
        chatbotPanel->setLastRecommendationSummary("");
    }
}

void Application::applyRecommendedRoute(const std::vector<int>& order) {
    const MapGraph& graph = wasteSystem->getGraph();
    std::vector<int> sanitized;
    sanitized.reserve(order.size());
    for (int nodeId : order) {
        if (graph.findNodeIndex(nodeId) >= 0) {
            sanitized.push_back(nodeId);
        }
    }
    if (sanitized.size() < 2) {
        chatbotPanel->setLastRecommendationSummary("route ignored (invalid ids)");
        return;
    }

    RouteResult aiResult;
    aiResult.algorithmName = "AI (Gemini)";
    aiResult.visitOrder = sanitized;
    aiResult.totalDistance = graph.calculateRouteDistance(sanitized);
    aiResult.wasteCollected = wasteSystem->computeWasteCollected(sanitized);
    aiResult.runtimeMs = 0.0;
    wasteSystem->populateCosts(aiResult);

    std::ostringstream summary;
    summary << (sanitized.size() - 1) << " stops, "
            << aiResult.totalDistance << " km, RM "
            << aiResult.totalCost;
    chatbotPanel->setLastRecommendationSummary(summary.str());

    wasteSystem->getEventLog().addEvent(
        "AI recommended route applied: " + summary.str());

    loadMissionRoute(aiResult, true);
}
