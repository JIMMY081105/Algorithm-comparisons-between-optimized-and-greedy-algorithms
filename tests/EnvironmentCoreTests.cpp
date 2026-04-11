#include "algorithms/ComparisonManager.h"
#include "core/MapGraph.h"
#include "core/WasteNode.h"
#include "core/WasteSystem.h"
#include "visualization/CityThemeRenderer.h"
#include "visualization/SeaThemeRenderer.h"

#include <cassert>
#include <cmath>
#include <iostream>

namespace {

bool nearlyEqual(float a, float b, float epsilon = 0.001f) {
    return std::fabs(a - b) <= epsilon;
}

int firstEligibleNode(const WasteSystem& system) {
    const auto eligible = system.getEligibleNodes(0.0f);
    assert(!eligible.empty());
    return eligible.front();
}

void testWeightedMatrixInstall() {
    MapGraph graph;
    graph.addNode(WasteNode(0, "HQ", 0.0f, 0.0f, 0.0f, true));
    graph.addNode(WasteNode(1, "A", 1.0f, 0.0f, 100.0f, false));
    graph.addNode(WasteNode(2, "B", 2.0f, 0.0f, 100.0f, false));

    graph.installWeightedMatrix({
        {0.0f, 2.0f, 5.0f},
        {2.0f, 0.0f, 3.0f},
        {5.0f, 3.0f, 0.0f},
    });

    assert(nearlyEqual(graph.calculateRouteDistance({0, 1, 2, 0}), 10.0f));
}

void testCityPresentationUsesExpandedStreetPath() {
    WasteSystem system;
    system.initializeMap();
    system.generateNewDayWithSeed(42u);

    CityThemeRenderer city;
    city.rebuildScene(system.getGraph(), system.getCurrentSeed());
    city.applyRouteWeights(system.getGraph());

    const int hqId = system.getGraph().getHQNode().getId();
    const int targetId = firstEligibleNode(system);
    RouteResult route;
    route.algorithmName = "Test";
    route.visitOrder = {hqId, targetId, hqId};

    const MissionPresentation presentation =
        city.buildMissionPresentation(route, system.getGraph());

    assert(presentation.isValid());
    assert(presentation.playbackPath.points.size() > route.visitOrder.size());
    assert(!presentation.legInsights.empty());
    assert(presentation.legInsights.front().totalCost >=
           presentation.legInsights.front().baseDistance);

    int orthogonalSegments = 0;
    for (std::size_t i = 1; i < presentation.playbackPath.points.size(); ++i) {
        const auto& a = presentation.playbackPath.points[i - 1];
        const auto& b = presentation.playbackPath.points[i];
        if (nearlyEqual(a.x, b.x, 0.01f) || nearlyEqual(a.y, b.y, 0.01f)) {
            ++orthogonalSegments;
        }
    }
    assert(orthogonalSegments >= 2);
}

void testWeatherPenaltiesStayNonNegative() {
    WasteSystem system;
    system.initializeMap();
    system.generateNewDayWithSeed(99u);

    CityThemeRenderer city;
    city.rebuildScene(system.getGraph(), system.getCurrentSeed());

    MapGraph sunnyGraph = system.getGraph();
    city.setWeather(CityWeather::Sunny);
    city.applyRouteWeights(sunnyGraph);

    MapGraph stormGraph = system.getGraph();
    city.setWeather(CityWeather::Stormy);
    city.applyRouteWeights(stormGraph);

    for (int i = 0; i < stormGraph.getNodeCount(); ++i) {
        for (int j = i + 1; j < stormGraph.getNodeCount(); ++j) {
            const float sunny = sunnyGraph.getAdjacencyMatrix()[i][j];
            const float storm = stormGraph.getAdjacencyMatrix()[i][j];
            assert(sunny >= 0.0f);
            assert(storm >= 0.0f);
            assert(storm >= sunny);
        }
    }
}

void testAlgorithmsReturnClosedTours() {
    WasteSystem system;
    system.initializeMap();
    system.generateNewDayWithSeed(77u);

    SeaThemeRenderer sea;
    sea.rebuildScene(system.getGraph(), system.getCurrentSeed());
    sea.applyRouteWeights(system.getGraph());

    ComparisonManager comparisonManager;
    comparisonManager.initializeAlgorithms();
    const int hqId = system.getGraph().getHQNode().getId();

    for (int i = 0; i < comparisonManager.getAlgorithmCount(); ++i) {
        const RouteResult result = comparisonManager.runSingleAlgorithm(i, system);
        if (!result.visitOrder.empty()) {
            assert(result.visitOrder.front() == hqId);
            assert(result.visitOrder.back() == hqId);
        }
    }
}

void testThemeWeightSwitchPreservesSimulationDay() {
    WasteSystem system;
    system.initializeMap();
    system.generateNewDayWithSeed(2024u);
    const int dayBefore = system.getDayNumber();
    const unsigned int seedBefore = system.getCurrentSeed();

    SeaThemeRenderer sea;
    sea.rebuildScene(system.getGraph(), system.getCurrentSeed());
    sea.applyRouteWeights(system.getGraph());
    const float seaDistance = system.getGraph().getDistance(0, 1);

    CityThemeRenderer city;
    city.rebuildScene(system.getGraph(), system.getCurrentSeed());
    city.applyRouteWeights(system.getGraph());
    const float cityDistance = system.getGraph().getDistance(0, 1);

    assert(system.getDayNumber() == dayBefore);
    assert(system.getCurrentSeed() == seedBefore);
    assert(!nearlyEqual(seaDistance, cityDistance, 0.01f));
}

} // namespace

int main() {
    testWeightedMatrixInstall();
    testCityPresentationUsesExpandedStreetPath();
    testWeatherPenaltiesStayNonNegative();
    testAlgorithmsReturnClosedTours();
    testThemeWeightSwitchPreservesSimulationDay();

    std::cout << "EnvironmentCoreTests passed" << std::endl;
    return 0;
}
