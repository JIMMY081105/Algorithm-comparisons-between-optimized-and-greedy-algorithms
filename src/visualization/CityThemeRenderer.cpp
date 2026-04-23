#include "visualization/CityThemeRendererInternal.h"

CityThemeRenderer::CityThemeRenderer()
    : dashboardInfo{EnvironmentTheme::City, "City", "Operational urban district",
                    CitySeason::Spring, "Spring", "Stormy",
                    "Storm-driven logistics pressure", 0.0f, 0, true, true},
      layoutSeed(0),
      season(CitySeason::Spring),
      weather(CityWeather::Stormy),
      trafficClock(0.0f),
      gridColumns(0),
      gridRows(0),
      primaryBoulevardColumn(0),
      primaryBoulevardRow(0),
      sceneMinX(0.0f),
      sceneMaxX(0.0f),
      sceneMinY(0.0f),
      sceneMaxY(0.0f),
      peripheralMinX(0.0f),
      peripheralMaxX(0.0f),
      peripheralMinY(0.0f),
      peripheralMaxY(0.0f),
      nodeCentroidX(0.0f),
      nodeCentroidY(0.0f),
      operationalCenterX(0.0f),
      operationalCenterY(0.0f),
      operationalRadiusX(1.0f),
      operationalRadiusY(1.0f),
      activeGraph(nullptr),
      staticBatchProjectionValid(false),
      staticBatchTileWidth(0.0f),
      staticBatchTileHeight(0.0f),
      staticBatchOffsetX(0.0f),
      staticBatchOffsetY(0.0f) {}

EnvironmentTheme CityThemeRenderer::getTheme() const {
    return EnvironmentTheme::City;
}

bool CityThemeRenderer::init() {
    return true;
}

void CityThemeRenderer::markStaticBatchesDirty() {
    groundBatch.dirty = true;
    decorativeBatch.dirty = true;
    transitBatch.dirty = true;
    mountainBatch.dirty = true;
}

void CityThemeRenderer::ensureStaticBatchProjectionCurrent() {
    const RenderUtils::ProjectionState& projection = RenderUtils::getProjection();
    const bool changed =
        !staticBatchProjectionValid ||
        std::abs(staticBatchTileWidth - projection.tileWidth) > 0.001f ||
        std::abs(staticBatchTileHeight - projection.tileHeight) > 0.001f ||
        std::abs(staticBatchOffsetX - projection.offsetX) > 0.001f ||
        std::abs(staticBatchOffsetY - projection.offsetY) > 0.001f;

    if (!changed) {
        return;
    }

    staticBatchTileWidth = projection.tileWidth;
    staticBatchTileHeight = projection.tileHeight;
    staticBatchOffsetX = projection.offsetX;
    staticBatchOffsetY = projection.offsetY;
    staticBatchProjectionValid = true;
    markStaticBatchesDirty();
}

void CityThemeRenderer::rebuildScene(const MapGraph& graph, unsigned int seed) {
    layoutSeed = seed;
    activeGraph = &graph;

    std::mt19937 networkRng(seed ^ 0x43A9D17u);
    generateGridNetwork(graph, networkRng);
    assignNodeAnchors(graph);
    updateSceneFocus(graph);

    std::mt19937 layoutRng(seed ^ 0xD741B29u);
    generateBlocks(layoutRng);
    buildings.clear();
    landscapeFeatures.clear();
    populateFromAssetLibrary(graph, layoutRng);
    generatePeripheralScene(layoutRng);
    refreshSeasonalRoadState();
    generateAmbientCars(layoutRng);

    refreshPairRoutes(graph);
    refreshDashboardInfo();
    markStaticBatchesDirty();
}

void CityThemeRenderer::update(float deltaTime) {
    trafficClock += deltaTime;
    for (auto& car : ambientCars) {
        car.offset += car.speed * deltaTime;
        if (car.offset > 1.0f) {
            car.offset -= std::floor(car.offset);
        }
    }
}

void CityThemeRenderer::applyRouteWeights(MapGraph& graph) {
    refreshPairRoutes(graph);

    const int nodeCount = graph.getNodeCount();
    std::vector<std::vector<float>> matrix(
        nodeCount, std::vector<float>(nodeCount, 0.0f));

    for (int i = 0; i < nodeCount; ++i) {
        for (int j = i + 1; j < nodeCount; ++j) {
            float weight = 0.0f;
            if (i < static_cast<int>(pairRoutes.size()) &&
                j < static_cast<int>(pairRoutes[i].size()) &&
                pairRoutes[i][j].valid) {
                weight = pairRoutes[i][j].insight.totalCost;
            } else {
                const WasteNode& a = graph.getNode(i);
                const WasteNode& b = graph.getNode(j);
                weight = pointDistance(a.getWorldX(), a.getWorldY(),
                                       b.getWorldX(), b.getWorldY()) * 1.8f;
            }

            matrix[i][j] = weight;
            matrix[j][i] = weight;
        }
    }

    graph.installWeightedMatrix(matrix);
}

MissionPresentation CityThemeRenderer::buildMissionPresentation(
    const RouteResult& route,
    const MapGraph& graph) const {
    MissionPresentation presentation;
    presentation.route = route;
    if (!route.isValid() || route.visitOrder.size() < 2) {
        return presentation;
    }

    std::vector<PlaybackPoint> pathPoints;
    std::vector<int> stopNodeIds;
    std::vector<std::size_t> stopPointIndices;

    for (std::size_t leg = 0; leg + 1 < route.visitOrder.size(); ++leg) {
        const int fromIndex = graph.findNodeIndex(route.visitOrder[leg]);
        const int toIndex = graph.findNodeIndex(route.visitOrder[leg + 1]);
        if (fromIndex < 0 || toIndex < 0 ||
            fromIndex >= static_cast<int>(pairRoutes.size()) ||
            toIndex >= static_cast<int>(pairRoutes[fromIndex].size())) {
            continue;
        }

        const PairRouteData& pair = pairRoutes[fromIndex][toIndex];
        if (!pair.valid || pair.polyline.empty()) {
            continue;
        }

        if (pathPoints.empty()) {
            pathPoints.push_back(pair.polyline.front());
        }

        for (std::size_t pointIndex = 1; pointIndex < pair.polyline.size(); ++pointIndex) {
            const PlaybackPoint& nextPoint = pair.polyline[pointIndex];
            if (pointDistance(pathPoints.back().x, pathPoints.back().y,
                              nextPoint.x, nextPoint.y) <= 0.001f) {
                continue;
            }

            pathPoints.push_back(nextPoint);
            if (pointIndex - 1 < pair.segmentSpeedFactors.size()) {
                presentation.playbackPath.segmentSpeedFactors.push_back(
                    pair.segmentSpeedFactors[pointIndex - 1]);
            } else {
                presentation.playbackPath.segmentSpeedFactors.push_back(1.0f);
            }
        }

        stopNodeIds.push_back(route.visitOrder[leg + 1]);
        stopPointIndices.push_back(pathPoints.empty() ? 0 : pathPoints.size() - 1);
        presentation.legInsights.push_back(pair.insight);
    }

    const int startIndex = graph.findNodeIndex(route.visitOrder.front());
    if (startIndex >= 0) {
        const WasteNode& startNode = graph.getNode(startIndex);
        const PlaybackPoint startPoint{startNode.getWorldX(), startNode.getWorldY()};
        if (pathPoints.empty() ||
            pointDistance(pathPoints.front().x, pathPoints.front().y,
                          startPoint.x, startPoint.y) > 0.001f) {
            pathPoints.insert(pathPoints.begin(), startPoint);
            for (std::size_t i = 0; i < stopPointIndices.size(); ++i) {
                ++stopPointIndices[i];
            }
        }
        stopNodeIds.insert(stopNodeIds.begin(), route.visitOrder.front());
        stopPointIndices.insert(stopPointIndices.begin(), 0);
    }

    presentation.playbackPath = MissionPresentationUtils::buildPlaybackPath(
        pathPoints, stopNodeIds, stopPointIndices,
        presentation.playbackPath.segmentSpeedFactors);

    const float avgCongestion = dashboardInfo.congestionLevel * 100.0f;
    presentation.narrative =
        std::string("City mode follows the district street graph through commercial, residential, campus, park, and landmark zones during ") +
        dashboardInfo.seasonLabel + " under " + dashboardInfo.weatherLabel +
        " conditions. Average live congestion load is " +
        std::to_string(static_cast<int>(avgCongestion)) + "%.";
    return presentation;
}

ThemeDashboardInfo CityThemeRenderer::getDashboardInfo() const {
    return dashboardInfo;
}

void CityThemeRenderer::setLayerToggles(const SceneLayerToggles& toggles) {
    layerToggles = toggles;
}

bool CityThemeRenderer::supportsWeather() const {
    return true;
}

CitySeason CityThemeRenderer::getSeason() const {
    return season;
}

void CityThemeRenderer::setSeason(CitySeason newSeason) {
    season = newSeason;
    refreshSeasonalRoadState();
    markStaticBatchesDirty();
    std::mt19937 carRng(layoutSeed ^ 0x41C7A93u ^
                        static_cast<unsigned int>(season) * 131u ^
                        static_cast<unsigned int>(weather) * 257u);
    generateAmbientCars(carRng);
    if (activeGraph != nullptr) {
        refreshPairRoutes(*activeGraph);
        refreshDashboardInfo();
    }
}

CityWeather CityThemeRenderer::getWeather() const {
    return weather;
}

void CityThemeRenderer::setWeather(CityWeather newWeather) {
    weather = newWeather;
    refreshSeasonalRoadState();
    markStaticBatchesDirty();
    std::mt19937 carRng(layoutSeed ^ 0x41C7A93u ^
                        static_cast<unsigned int>(season) * 131u ^
                        static_cast<unsigned int>(weather) * 257u);
    generateAmbientCars(carRng);
    if (activeGraph != nullptr) {
        refreshPairRoutes(*activeGraph);
        refreshDashboardInfo();
    }
}

void CityThemeRenderer::randomizeTrafficConditions(unsigned int seed) {
    std::mt19937 trafficRng(seed ^ 0x6D2F41Bu);
    applyTrafficConditions(trafficRng);
    refreshSeasonalRoadState();
    markStaticBatchesDirty();

    std::mt19937 carRng(seed ^ 0x41C7A93u);
    generateAmbientCars(carRng);

    if (activeGraph != nullptr) {
        refreshPairRoutes(*activeGraph);
        refreshDashboardInfo();
    }
}

void CityThemeRenderer::randomizeWeather(unsigned int seed) {
    std::mt19937 rng(seed ^ 0xB3E947u);
    setWeather(nextDistinctWeatherForSeason(season, weather, rng));
}


void CityThemeRenderer::cleanup() {
    groundBatch.destroy();
    decorativeBatch.destroy();
    transitBatch.destroy();
    mountainBatch.destroy();
    staticBatchProjectionValid = false;

    presetBuildings.clear();
    presetEnvironments.clear();
    presetVehicles.clear();
    presetRoadProps.clear();
}
