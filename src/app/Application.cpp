#include "app/Application.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <iostream>
#include <stdexcept>

// GLFW error callback — logs errors to stderr
static void glfwErrorCallback(int error, const char* description) {
    std::cerr << "GLFW Error " << error << ": " << description << std::endl;
}

Application::Application()
    : window(nullptr), windowWidth(1280), windowHeight(850),
      lastFrameTime(0.0f), initialized(false) {}

Application::~Application() {
    if (initialized) {
        shutdown();
    }
}

bool Application::init() {
    try {
        if (!initWindow()) return false;
        if (!initOpenGL()) return false;
        initImGui();
        initSimulation();
        initialized = true;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Initialization failed: " << e.what() << std::endl;
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
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    GLFWmonitor* primaryMonitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* videoMode =
        (primaryMonitor != nullptr) ? glfwGetVideoMode(primaryMonitor) : nullptr;

    const char* title = "Smart Waste Clearance System - EcoRoute Solutions";
    if (primaryMonitor != nullptr && videoMode != nullptr) {
        windowWidth = videoMode->width;
        windowHeight = videoMode->height;
        window = glfwCreateWindow(windowWidth, windowHeight, title,
                                  primaryMonitor, nullptr);
    } else {
        window = glfwCreateWindow(windowWidth, windowHeight, title,
                                  nullptr, nullptr);
    }
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);  // enable vsync for smooth animation

    return true;
}

bool Application::initOpenGL() {
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return false;
    }

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

    // Professional dark theme with teal/cyan accents
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 6.0f;
    style.FrameRounding = 4.0f;
    style.GrabRounding = 3.0f;
    style.ScrollbarRounding = 4.0f;
    style.FramePadding = ImVec2(8, 4);
    style.ItemSpacing = ImVec2(8, 6);

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg]       = ImVec4(0.10f, 0.10f, 0.13f, 0.95f);
    colors[ImGuiCol_TitleBg]        = ImVec4(0.08f, 0.08f, 0.10f, 1.00f);
    colors[ImGuiCol_TitleBgActive]  = ImVec4(0.12f, 0.18f, 0.24f, 1.00f);
    colors[ImGuiCol_Header]         = ImVec4(0.15f, 0.22f, 0.30f, 0.80f);
    colors[ImGuiCol_HeaderHovered]  = ImVec4(0.20f, 0.30f, 0.40f, 0.80f);
    colors[ImGuiCol_Button]         = ImVec4(0.15f, 0.25f, 0.35f, 1.00f);
    colors[ImGuiCol_ButtonHovered]  = ImVec4(0.20f, 0.35f, 0.50f, 1.00f);
    colors[ImGuiCol_ButtonActive]   = ImVec4(0.10f, 0.40f, 0.55f, 1.00f);
    colors[ImGuiCol_FrameBg]        = ImVec4(0.12f, 0.14f, 0.18f, 1.00f);
    colors[ImGuiCol_SliderGrab]     = ImVec4(0.20f, 0.60f, 0.80f, 1.00f);
    colors[ImGuiCol_CheckMark]      = ImVec4(0.20f, 0.70f, 0.90f, 1.00f);
    colors[ImGuiCol_SeparatorActive]= ImVec4(0.15f, 0.50f, 0.70f, 1.00f);
    colors[ImGuiCol_Tab]            = ImVec4(0.12f, 0.18f, 0.24f, 1.00f);
    colors[ImGuiCol_TabHovered]     = ImVec4(0.20f, 0.35f, 0.50f, 1.00f);
    colors[ImGuiCol_TableHeaderBg]  = ImVec4(0.14f, 0.18f, 0.24f, 1.00f);

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");
}

void Application::initSimulation() {
    wasteSystem.initializeMap();
    comparisonManager.initializeAlgorithms();

    // Generate the first day immediately so the user sees waste data
    wasteSystem.generateNewDay();

    lastFrameTime = static_cast<float>(glfwGetTime());
}

void Application::run() {
    if (!initialized) {
        std::cerr << "Application not initialized" << std::endl;
        return;
    }

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // Delta time for frame-rate independent animation
        float currentTime = static_cast<float>(glfwGetTime());
        float deltaTime = currentTime - lastFrameTime;
        lastFrameTime = currentTime;

        // Prevent large dt spikes from pauses/breakpoints
        if (deltaTime > 0.1f) deltaTime = 0.1f;

        update(deltaTime);
        renderFrame();

        glfwSwapBuffers(window);
    }
}

void Application::update(float deltaTime) {
    // Advance the truck along the current route
    int collectedNodeId = animController.update(deltaTime);

    // When the truck arrives at a node, mark it collected and log it
    if (collectedNodeId >= 0) {
        wasteSystem.markNodeCollected(collectedNodeId);

        int idx = wasteSystem.getGraph().findNodeIndex(collectedNodeId);
        if (idx >= 0) {
            const WasteNode& node = wasteSystem.getGraph().getNode(idx);
            if (!node.getIsHQ()) {
                wasteSystem.getEventLog().addEvent(
                    "Collected waste at " + node.getName());
            }
        }
    }

    // Keep the renderer's route line progress in sync with animation
    renderer.setRouteDrawProgress(animController.getRouteDrawSegment());
}

void Application::renderFrame() {
    glfwGetFramebufferSize(window, &windowWidth, &windowHeight);
    glViewport(0, 0, windowWidth, windowHeight);
    RenderUtils::updateProjection(static_cast<float>(windowWidth),
                                  static_cast<float>(windowHeight));

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

    // Render the isometric map with OpenGL
    float dt = 1.0f / 60.0f;
    renderer.render(wasteSystem.getGraph(), animController.getTruck(),
                    animController.getCurrentRoute(), dt);

    // Render ImGui dashboard on top
    beginImGuiFrame();

    auto actions = dashboardUI.render(wasteSystem, comparisonManager,
                                       animController, currentResult);

    // Process all UI actions
    handleUIActions(actions);

    endImGuiFrame();
}

void Application::handleUIActions(const DashboardUI::UIActions& actions) {
    // Generate New Day — randomize waste levels
    if (actions.generateNewDay) {
        wasteSystem.generateNewDay();
        currentResult = RouteResult();
        animController.stop();
        renderer.resetAnimation();
        comparisonManager.clearResults();
    }

    // Run a single selected algorithm
    if (actions.runSelectedAlgorithm && actions.algorithmToRun >= 0) {
        try {
            currentResult = comparisonManager.runSingleAlgorithm(
                actions.algorithmToRun, wasteSystem);
            animController.loadRoute(currentResult, wasteSystem.getGraph());
            animController.play();
            wasteSystem.resetCollectionStatus();
        } catch (const std::exception& e) {
            wasteSystem.getEventLog().addEvent(
                std::string("Error: ") + e.what());
        }
    }

    // Compare all four algorithms
    if (actions.compareAll) {
        try {
            comparisonManager.runAllAlgorithms(wasteSystem);

            // Load the best algorithm's route for animation
            int bestIdx = comparisonManager.getBestAlgorithmIndex();
            if (bestIdx >= 0) {
                const auto& results = comparisonManager.getResults();
                currentResult = results[bestIdx];
                animController.loadRoute(currentResult, wasteSystem.getGraph());
            }
        } catch (const std::exception& e) {
            wasteSystem.getEventLog().addEvent(
                std::string("Comparison error: ") + e.what());
        }
    }

    // Play / start animation
    if (actions.playPause) {
        if (currentResult.isValid()) {
            if (animController.isFinished()) {
                animController.replay(wasteSystem.getGraph());
                wasteSystem.resetCollectionStatus();
            } else {
                animController.play();
                wasteSystem.resetCollectionStatus();
            }
        }
    }

    // Replay animation from start
    if (actions.replay) {
        if (currentResult.isValid()) {
            wasteSystem.resetCollectionStatus();
            animController.replay(wasteSystem.getGraph());
        }
    }

    // Export summary to TXT
    if (actions.exportResults && currentResult.isValid()) {
        try {
            std::string file = resultLogger.logCurrentResult(
                currentResult, wasteSystem);
            wasteSystem.getEventLog().addEvent("Saved: " + file);
        } catch (const std::exception& e) {
            wasteSystem.getEventLog().addEvent(
                std::string("Export error: ") + e.what());
        }
    }

    // Export comparison to CSV
    if (actions.exportComparison) {
        try {
            std::string file = resultLogger.logComparison(
                comparisonManager, wasteSystem);
            if (!file.empty()) {
                wasteSystem.getEventLog().addEvent("Saved: " + file);
            }
        } catch (const std::exception& e) {
            wasteSystem.getEventLog().addEvent(
                std::string("Export error: ") + e.what());
        }
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
    if (!initialized) return;

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    if (window) {
        glfwDestroyWindow(window);
        window = nullptr;
    }
    glfwTerminate();

    initialized = false;
}
