#include "algorithms/ComparisonManager.h"
#include "core/CostModel.h"
#include "core/EventLog.h"
#include "core/MapGraph.h"
#include "core/RoadEvent.h"
#include "core/TollStation.h"
#include "core/WasteNode.h"
#include "core/WasteSystem.h"
#include "environment/EnvironmentController.h"
#include "visualization/CityThemeRenderer.h"
#include "visualization/SeaThemeRenderer.h"

#include <cassert>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {

bool nearlyEqual(float a, float b, float epsilon = 0.001f) {
    return std::fabs(a - b) <= epsilon;
}

int firstEligibleNode(const WasteSystem& system) {
    const auto eligible = system.getEligibleNodes(0.0f);
    assert(!eligible.empty());
    return eligible.front();
}

RouteResult buildDeterministicRoute(const MapGraph& graph) {
    assert(graph.getNodeCount() > 1);

    RouteResult route;
    route.algorithmName = "Deterministic";
    route.visitOrder = {
        graph.getHQNode().getId(),
        graph.getNode(1).getId(),
        graph.getHQNode().getId(),
    };
    return route;
}

void assertMatricesMatch(const MapGraph& lhs, const MapGraph& rhs) {
    assert(lhs.getAdjacencyMatrix().size() == rhs.getAdjacencyMatrix().size());
    for (std::size_t row = 0; row < lhs.getAdjacencyMatrix().size(); ++row) {
        assert(lhs.getAdjacencyMatrix()[row].size() == rhs.getAdjacencyMatrix()[row].size());
        for (std::size_t col = 0; col < lhs.getAdjacencyMatrix()[row].size(); ++col) {
            assert(nearlyEqual(lhs.getAdjacencyMatrix()[row][col],
                               rhs.getAdjacencyMatrix()[row][col]));
        }
    }
}

bool matricesDiffer(const MapGraph& lhs, const MapGraph& rhs, float epsilon = 0.001f) {
    if (lhs.getAdjacencyMatrix().size() != rhs.getAdjacencyMatrix().size()) {
        return true;
    }

    for (std::size_t row = 0; row < lhs.getAdjacencyMatrix().size(); ++row) {
        if (lhs.getAdjacencyMatrix()[row].size() != rhs.getAdjacencyMatrix()[row].size()) {
            return true;
        }

        for (std::size_t col = 0; col < lhs.getAdjacencyMatrix()[row].size(); ++col) {
            if (!nearlyEqual(lhs.getAdjacencyMatrix()[row][col],
                             rhs.getAdjacencyMatrix()[row][col],
                             epsilon)) {
                return true;
            }
        }
    }

    return false;
}

void assertPresentationMatches(const MissionPresentation& lhs,
                               const MissionPresentation& rhs) {
    assert(lhs.isValid());
    assert(rhs.isValid());
    assert(lhs.playbackPath.points.size() == rhs.playbackPath.points.size());
    assert(lhs.playbackPath.segmentSpeedFactors.size() ==
           rhs.playbackPath.segmentSpeedFactors.size());
    assert(lhs.legInsights.size() == rhs.legInsights.size());

    for (std::size_t i = 0; i < lhs.playbackPath.points.size(); ++i) {
        assert(nearlyEqual(lhs.playbackPath.points[i].x, rhs.playbackPath.points[i].x));
        assert(nearlyEqual(lhs.playbackPath.points[i].y, rhs.playbackPath.points[i].y));
    }

    for (std::size_t i = 0; i < lhs.playbackPath.segmentSpeedFactors.size(); ++i) {
        assert(nearlyEqual(lhs.playbackPath.segmentSpeedFactors[i],
                           rhs.playbackPath.segmentSpeedFactors[i]));
    }

    for (std::size_t i = 0; i < lhs.legInsights.size(); ++i) {
        assert(nearlyEqual(lhs.legInsights[i].totalCost, rhs.legInsights[i].totalCost));
        assert(nearlyEqual(lhs.legInsights[i].baseDistance, rhs.legInsights[i].baseDistance));
    }
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

void testEventLogUniqueOwnershipBehavior() {
    EventLog log;
    log.addEventWithTime("10:00:00", "first");
    log.addEventWithTime("10:01:00", "second");
    log.addEventWithTime("10:02:00", "third");

    assert(log.getCount() == 3);
    assert(log.getHead() != nullptr);
    assert(log.getHead()->message == "first");
    assert(log.getHead()->nextEntry() != nullptr);
    assert(log.getHead()->nextEntry()->message == "second");

    const auto recent = log.getRecentEvents(2);
    assert(recent.size() == 2);
    assert(recent[0]->message == "third");
    assert(recent[1]->message == "second");

    log.clear();
    assert(log.getCount() == 0);
    assert(log.getHead() == nullptr);
    assert(log.getRecentEvents(5).empty());
}

void testTollStationEncapsulationAndValidation() {
    const TollStation toll(1, 2, 3.50f, "Gate");
    assert(toll.fromNodeId() == 1);
    assert(toll.toNodeId() == 2);
    assert(nearlyEqual(toll.fee(), 3.50f));
    assert(toll.name() == "Gate");
    assert(toll.isCrossedBy(1, 2));
    assert(toll.isCrossedBy(2, 1));
    assert(!toll.isCrossedBy(1, 3));

    bool threw = false;
    try {
        TollStation invalid(1, 1, 1.0f, "Bad");
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    assert(threw);

    threw = false;
    try {
        TollStation invalid(1, 2, -1.0f, "Bad");
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    assert(threw);

    threw = false;
    try {
        TollStation invalid(1, 2, 1.0f, "");
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    assert(threw);
}

void testRoadEventRules() {
    assert(!RoadEventRules::isBlocking(RoadEvent::NONE));
    assert(RoadEventRules::isBlocking(RoadEvent::FLOOD));
    assert(RoadEventRules::isBlocking(RoadEvent::FESTIVAL));
    assert(nearlyEqual(RoadEventRules::distanceMultiplier(RoadEvent::NONE),
                       RoadEventRules::kNormalDistanceMultiplier));
    assert(nearlyEqual(RoadEventRules::distanceMultiplier(RoadEvent::FLOOD),
                       RoadEventRules::kImpassableDistanceMultiplier));
    assert(nearlyEqual(RoadEventRules::speedFraction(RoadEvent::FESTIVAL),
                       RoadEventRules::kBlockedSpeedFraction));
    assert(nearlyEqual(RoadEventRules::fuelMultiplier(RoadEvent::FLOOD),
                       RoadEventRules::kDefaultFuelMultiplier));
    assert(std::string(RoadEventRules::label(RoadEvent::FLOOD)) == "FLOOD");
    assert(std::string(RoadEventRules::fullName(RoadEvent::FESTIVAL)) == "Festival");
}

void testCostModelEfficiencyBonusTiers() {
    const auto& tiers = CostModel::getEfficiencyBonusTiers();
    assert(tiers.size() == CostModel::kEfficiencyBonusTierCount);
    assert(nearlyEqual(tiers[0].distanceLimitKm, 80.0f));
    assert(nearlyEqual(tiers[0].bonusRm, 25.0f));

    CostModel costModel;
    assert(nearlyEqual(costModel.calculateEfficiencyBonus(79.99f), 25.0f));
    assert(nearlyEqual(costModel.calculateEfficiencyBonus(80.0f), 15.0f));
    assert(nearlyEqual(costModel.calculateEfficiencyBonus(119.99f), 15.0f));
    assert(nearlyEqual(costModel.calculateEfficiencyBonus(120.0f), 8.0f));
    assert(nearlyEqual(costModel.calculateEfficiencyBonus(179.99f), 8.0f));
    assert(nearlyEqual(costModel.calculateEfficiencyBonus(180.0f), 3.0f));
    assert(nearlyEqual(costModel.calculateEfficiencyBonus(249.99f), 3.0f));
    assert(nearlyEqual(costModel.calculateEfficiencyBonus(250.0f), 0.0f));
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

void testCityStartsStormyOnFirstBuild() {
    WasteSystem system;
    system.initializeMap();
    system.generateNewDayWithSeed(11u);

    CityThemeRenderer city;
    city.rebuildScene(system.getGraph(), system.getCurrentSeed());

    assert(city.getWeather() == CityWeather::Stormy);
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

    EnvironmentController controller;
    MapGraph weightedGraph = system.getGraph();
    controller.rebuildScenes(weightedGraph);
    controller.setActiveTheme(EnvironmentTheme::Sea, weightedGraph);
    controller.applyActiveWeights(weightedGraph);
    const float seaDistance = weightedGraph.getDistance(0, 1);

    controller.setActiveTheme(EnvironmentTheme::City, weightedGraph);
    const float cityDistance = weightedGraph.getDistance(0, 1);

    controller.setActiveTheme(EnvironmentTheme::Sea, weightedGraph);
    const float seaDistanceAgain = weightedGraph.getDistance(0, 1);

    assert(system.getDayNumber() == dayBefore);
    assert(system.getCurrentSeed() == seedBefore);
    assert(!nearlyEqual(seaDistance, cityDistance, 0.01f));
    assert(nearlyEqual(seaDistance, seaDistanceAgain, 0.01f));
}

void testEnvironmentSceneIsStableAcrossDaySeeds() {
    WasteSystem firstDay;
    firstDay.initializeMap();
    firstDay.generateNewDayWithSeed(31415u);

    WasteSystem secondDay;
    secondDay.initializeMap();
    secondDay.generateNewDayWithSeed(27182u);

    MapGraph firstGraph = firstDay.getGraph();
    EnvironmentController firstController;
    firstController.rebuildScenes(firstGraph);
    firstController.setActiveTheme(EnvironmentTheme::City, firstGraph);
    firstController.activeRenderer().setWeather(CityWeather::Rainy);
    firstController.applyActiveWeights(firstGraph);
    const MissionPresentation firstPresentation =
        firstController.buildMissionPresentation(buildDeterministicRoute(firstGraph),
                                                firstGraph);

    MapGraph secondGraph = secondDay.getGraph();
    EnvironmentController secondController;
    secondController.rebuildScenes(secondGraph);
    secondController.setActiveTheme(EnvironmentTheme::City, secondGraph);
    secondController.activeRenderer().setWeather(CityWeather::Rainy);
    secondController.applyActiveWeights(secondGraph);
    const MissionPresentation secondPresentation =
        secondController.buildMissionPresentation(buildDeterministicRoute(secondGraph),
                                                 secondGraph);

    assertMatricesMatch(firstGraph, secondGraph);
    assertPresentationMatches(firstPresentation, secondPresentation);
}

void testCityTrafficChangesAcrossNewDaysWhenWeatherNormalized() {
    WasteSystem system;
    system.initializeMap();

    EnvironmentController controller;
    controller.rebuildScenes(system.getGraph());
    controller.setActiveTheme(EnvironmentTheme::City, system.getGraph());

    system.generateNewDayWithSeed(1001u);
    controller.randomizeCityTraffic(1001u, system.getGraph());
    controller.activeRenderer().setWeather(CityWeather::Cloudy);
    controller.applyActiveWeights(system.getGraph());
    const MapGraph firstWeightedGraph = system.getGraph();

    system.generateNewDayWithSeed(2002u);
    controller.randomizeCityTraffic(2002u, system.getGraph());
    controller.activeRenderer().setWeather(CityWeather::Cloudy);
    controller.applyActiveWeights(system.getGraph());

    assert(matricesDiffer(firstWeightedGraph, system.getGraph()));
}

void testWeatherRefreshKeepsTrafficWhenWeatherNormalized() {
    WasteSystem system;
    system.initializeMap();

    EnvironmentController controller;
    controller.rebuildScenes(system.getGraph());
    controller.setActiveTheme(EnvironmentTheme::City, system.getGraph());

    system.generateNewDayWithSeed(404u);
    controller.randomizeCityTraffic(404u, system.getGraph());
    controller.activeRenderer().setWeather(CityWeather::Cloudy);
    controller.applyActiveWeights(system.getGraph());
    const MapGraph baselineGraph = system.getGraph();

    controller.randomizeCityWeather(909u, system.getGraph());
    controller.activeRenderer().setWeather(CityWeather::Cloudy);
    controller.applyActiveWeights(system.getGraph());

    assertMatricesMatch(baselineGraph, system.getGraph());
}

void testCitySceneIsDeterministicForSeed() {
    WasteSystem system;
    system.initializeMap();
    system.generateNewDayWithSeed(31415u);

    CityThemeRenderer cityA;
    cityA.rebuildScene(system.getGraph(), system.getCurrentSeed());
    cityA.setWeather(CityWeather::Rainy);

    CityThemeRenderer cityB;
    cityB.rebuildScene(system.getGraph(), system.getCurrentSeed());
    cityB.setWeather(CityWeather::Rainy);

    MapGraph weightedA = system.getGraph();
    cityA.applyRouteWeights(weightedA);
    MapGraph weightedB = system.getGraph();
    cityB.applyRouteWeights(weightedB);

    assert(weightedA.getAdjacencyMatrix().size() == weightedB.getAdjacencyMatrix().size());
    for (std::size_t row = 0; row < weightedA.getAdjacencyMatrix().size(); ++row) {
        for (std::size_t col = 0; col < weightedA.getAdjacencyMatrix()[row].size(); ++col) {
            assert(nearlyEqual(weightedA.getAdjacencyMatrix()[row][col],
                               weightedB.getAdjacencyMatrix()[row][col]));
        }
    }

    const int hqId = system.getGraph().getHQNode().getId();
    const int targetId = firstEligibleNode(system);
    RouteResult route;
    route.algorithmName = "Deterministic";
    route.visitOrder = {hqId, targetId, hqId};

    const MissionPresentation presentationA =
        cityA.buildMissionPresentation(route, system.getGraph());
    const MissionPresentation presentationB =
        cityB.buildMissionPresentation(route, system.getGraph());

    assertPresentationMatches(presentationA, presentationB);
}

} // namespace

int main() {
    testWeightedMatrixInstall();
    testEventLogUniqueOwnershipBehavior();
    testTollStationEncapsulationAndValidation();
    testRoadEventRules();
    testCostModelEfficiencyBonusTiers();
    testCityPresentationUsesExpandedStreetPath();
    testCityStartsStormyOnFirstBuild();
    testWeatherPenaltiesStayNonNegative();
    testAlgorithmsReturnClosedTours();
    testThemeWeightSwitchPreservesSimulationDay();
    testEnvironmentSceneIsStableAcrossDaySeeds();
    testCityTrafficChangesAcrossNewDaysWhenWeatherNormalized();
    testWeatherRefreshKeepsTrafficWhenWeatherNormalized();
    testCitySceneIsDeterministicForSeed();

    std::cout << "EnvironmentCoreTests passed" << std::endl;
    return 0;
}
