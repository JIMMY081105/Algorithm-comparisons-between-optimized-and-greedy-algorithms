#include "visualization/SeaThemeRendererInternal.h"

SeaThemeRenderer::SeaThemeRenderer()
    : dashboardInfo{EnvironmentTheme::Sea, "Sea", "Harbour command",
                    CitySeason::Spring, "N/A", "N/A",
                    "Open-water transit and port logistics", 0.0f, 0, false, false},
      sceneSeed(0),
      currentGarbageSinkProgress(0.0f),
      boatCargoFillRatio(0.0f),
      boatWakeStrength(0.0f),
      boatPitchWave(0.0f),
      boatRollWave(0.0f),
      boatIsMoving(false),
      sceneBounds{0.0f, 0.0f, 0.0f, 0.0f},
      activeGraph(nullptr) {}

EnvironmentTheme SeaThemeRenderer::getTheme() const {
    return EnvironmentTheme::Sea;
}

bool SeaThemeRenderer::init() {
    return oceanShader.init();
}

void SeaThemeRenderer::rebuildScene(const MapGraph& graph, unsigned int seed) {
    sceneSeed = seed;
    activeGraph = &graph;
    updateSceneBounds(graph);
    dashboardInfo.subtitle =
        (seed % 2 == 0) ? "Harbour command" : "Offshore response deck";
    dashboardInfo.atmosphereLabel =
        (seed % 3 == 0) ? "Calm swells and clean marine lanes"
                        : "Rolling tides across the cleanup corridor";
}

void SeaThemeRenderer::update(float deltaTime) {
    currentGarbageSinkProgress = clamp01(currentGarbageSinkProgress + deltaTime * 0.2f);
}

void SeaThemeRenderer::applyRouteWeights(MapGraph& graph) {
    graph.buildFullyConnectedGraph();
}

MissionPresentation SeaThemeRenderer::buildMissionPresentation(
    const RouteResult& route,
    const MapGraph& graph) const {
    MissionPresentation presentation;
    presentation.route = route;
    if (!route.isValid()) {
        return presentation;
    }

    presentation.playbackPath = buildDirectPlaybackPath(route, graph);
    presentation.narrative =
        "Sea mode uses direct marine lanes between cleanup sectors for clear route comparison.";

    for (std::size_t i = 0; i + 1 < route.visitOrder.size(); ++i) {
        const int fromId = route.visitOrder[i];
        const int toId = route.visitOrder[i + 1];
        presentation.legInsights.push_back(RouteInsight{
            fromId,
            toId,
            graph.getDistance(fromId, toId),
            0.0f,
            0.0f,
            0.0f,
            0.0f,
            graph.getDistance(fromId, toId)
        });
    }

    return presentation;
}

ThemeDashboardInfo SeaThemeRenderer::getDashboardInfo() const {
    return dashboardInfo;
}

void SeaThemeRenderer::setLayerToggles(const SceneLayerToggles& toggles) {
    layerToggles = toggles;
}

bool SeaThemeRenderer::supportsWeather() const {
    return false;
}

CityWeather SeaThemeRenderer::getWeather() const {
    return CityWeather::Sunny;
}

void SeaThemeRenderer::setWeather(CityWeather) {}

void SeaThemeRenderer::randomizeWeather(unsigned int) {}

void SeaThemeRenderer::cleanup() {
    oceanShader.cleanup();
}

void SeaThemeRenderer::updateSceneBounds(const MapGraph& graph) {
    const auto& nodes = graph.getNodes();
    if (nodes.empty()) {
        sceneBounds = {0.0f, 0.0f, 0.0f, 0.0f};
        return;
    }

    float minWorldX = nodes.front().getWorldX();
    float maxWorldX = nodes.front().getWorldX();
    float minWorldY = nodes.front().getWorldY();
    float maxWorldY = nodes.front().getWorldY();

    for (const auto& node : nodes) {
        minWorldX = std::min(minWorldX, node.getWorldX());
        maxWorldX = std::max(maxWorldX, node.getWorldX());
        minWorldY = std::min(minWorldY, node.getWorldY());
        maxWorldY = std::max(maxWorldY, node.getWorldY());
    }

    sceneBounds = {minWorldX, maxWorldX, minWorldY, maxWorldY};
}

void SeaThemeRenderer::updateGarbageSinkState(const MapGraph& graph, float deltaTime) {
    std::unordered_map<int, float> nextState;
    nextState.reserve(graph.getNodeCount());

    for (const auto& node : graph.getNodes()) {
        float progress = 0.0f;
        const auto it = garbageSinkProgress.find(node.getId());
        if (it != garbageSinkProgress.end()) {
            progress = it->second;
        }

        if (node.isCollected()) {
            progress = clamp01(progress + deltaTime * 1.75f);
        } else {
            progress = clamp01(progress - deltaTime * 3.50f);
        }

        nextState[node.getId()] = progress;
    }

    garbageSinkProgress.swap(nextState);
}

float SeaThemeRenderer::getGarbageSinkState(int nodeId) {
    const auto it = garbageSinkProgress.find(nodeId);
    return (it != garbageSinkProgress.end()) ? it->second : 0.0f;
}

PlaybackPath SeaThemeRenderer::buildDirectPlaybackPath(const RouteResult& route,
                                                       const MapGraph& graph) const {
    std::vector<PlaybackPoint> points;
    std::vector<int> stopNodeIds;
    std::vector<std::size_t> stopPointIndices;

    points.reserve(route.visitOrder.size());
    stopNodeIds.reserve(route.visitOrder.size());
    stopPointIndices.reserve(route.visitOrder.size());

    for (int nodeId : route.visitOrder) {
        const int nodeIndex = graph.findNodeIndex(nodeId);
        if (nodeIndex < 0) {
            continue;
        }

        const WasteNode& node = graph.getNode(nodeIndex);
        points.push_back(PlaybackPoint{node.getWorldX(), node.getWorldY()});
        stopNodeIds.push_back(nodeId);
        stopPointIndices.push_back(points.size() - 1);
    }

    return MissionPresentationUtils::buildPlaybackPath(
        points, stopNodeIds, stopPointIndices);
}
