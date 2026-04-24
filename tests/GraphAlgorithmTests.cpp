#include "TestHarness.h"

#include "algorithms/BellmanFordRoute.h"
#include "algorithms/ComparisonManager.h"
#include "algorithms/DijkstraRoute.h"
#include "algorithms/FloydWarshallRoute.h"
#include "algorithms/GreedyRoute.h"
#include "algorithms/MSTRoute.h"
#include "algorithms/RegularRoute.h"
#include "algorithms/TSPRoute.h"
#include "core/MapGraph.h"
#include "core/RoadEvent.h"
#include "core/WasteSystem.h"

#include <algorithm>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

MapGraph makeFourNodeGraph() {
    MapGraph graph;
    graph.addNode(WasteNode(0, "HQ", 0.0f, 0.0f, 0.0f, true));
    graph.addNode(WasteNode(1, "A", 1.0f, 0.0f, 100.0f));
    graph.addNode(WasteNode(2, "B", 2.0f, 0.0f, 100.0f));
    graph.addNode(WasteNode(3, "C", 3.0f, 0.0f, 100.0f));
    graph.buildFullyConnectedGraph();
    graph.installWeightedMatrix({
        {0.0f, 2.0f, 9.0f, 10.0f},
        {2.0f, 0.0f, 3.0f, 4.0f},
        {9.0f, 3.0f, 0.0f, 1.0f},
        {10.0f, 4.0f, 1.0f, 0.0f},
    });
    return graph;
}

MapGraph makeTieFreeAlgorithmGraph() {
    MapGraph graph;
    graph.addNode(WasteNode(0, "HQ", 0.0f, 0.0f, 0.0f, true));
    graph.addNode(WasteNode(1, "A", 1.0f, 0.0f, 100.0f));
    graph.addNode(WasteNode(2, "B", 2.0f, 0.0f, 100.0f));
    graph.addNode(WasteNode(3, "C", 3.0f, 0.0f, 100.0f));
    graph.buildFullyConnectedGraph();
    graph.installWeightedMatrix({
        {0.0f, 1.0f, 4.0f, 8.0f},
        {1.0f, 0.0f, 2.0f, 7.0f},
        {4.0f, 2.0f, 0.0f, 3.0f},
        {8.0f, 7.0f, 3.0f, 0.0f},
    });
    return graph;
}

MapGraph makeFiveNodeGraphForInsertionAlgorithms() {
    MapGraph graph;
    graph.addNode(WasteNode(0, "HQ", 0.0f, 0.0f, 0.0f, true));
    graph.addNode(WasteNode(1, "A", 1.0f, 0.0f, 100.0f));
    graph.addNode(WasteNode(2, "B", 2.0f, 0.0f, 100.0f));
    graph.addNode(WasteNode(3, "C", 0.0f, 2.0f, 100.0f));
    graph.addNode(WasteNode(4, "D", 2.0f, 2.0f, 100.0f));
    graph.buildFullyConnectedGraph();
    graph.installWeightedMatrix({
        {0.0f, 2.0f, 9.0f, 6.0f, 7.0f},
        {2.0f, 0.0f, 3.0f, 4.0f, 8.0f},
        {9.0f, 3.0f, 0.0f, 5.0f, 1.0f},
        {6.0f, 4.0f, 5.0f, 0.0f, 2.0f},
        {7.0f, 8.0f, 1.0f, 2.0f, 0.0f},
    });
    return graph;
}

std::vector<int> eligible123() {
    return {1, 2, 3};
}

template <typename Algorithm>
RouteResult runAlgorithm(const MapGraph& graph,
                         const std::vector<int>& eligible = eligible123()) {
    Algorithm algorithm;
    return algorithm.computeRoute(graph, eligible, 0);
}

void requireRouteEquals(const RouteResult& result,
                        const std::vector<int>& expected) {
    REQUIRE_TRUE(result.visitOrder == expected);
}

void requireValidClosedRoute(const RouteResult& result,
                             const std::vector<int>& eligible,
                             int hqId = 0) {
    REQUIRE_TRUE(result.isValid());
    REQUIRE_EQ(result.visitOrder.front(), hqId);
    REQUIRE_EQ(result.visitOrder.back(), hqId);

    std::vector<int> interior;
    for (std::size_t i = 1; i + 1 < result.visitOrder.size(); ++i) {
        interior.push_back(result.visitOrder[i]);
    }
    std::sort(interior.begin(), interior.end());

    std::vector<int> sortedEligible = eligible;
    sortedEligible.erase(std::remove(sortedEligible.begin(), sortedEligible.end(), hqId),
                         sortedEligible.end());
    std::sort(sortedEligible.begin(), sortedEligible.end());
    REQUIRE_TRUE(interior == sortedEligible);
    REQUIRE_EQ(std::set<int>(interior.begin(), interior.end()).size(),
               interior.size());
}

float routeDistance(const MapGraph& graph, const RouteResult& result) {
    return graph.calculateRouteDistance(result.visitOrder);
}

} // namespace

TEST_CASE(MapGraphBuildsScaledEuclideanDistances) {
    MapGraph graph;
    graph.addNode(WasteNode(0, "HQ", 0.0f, 0.0f, 0.0f, true));
    graph.addNode(WasteNode(1, "A", 3.0f, 4.0f, 100.0f));
    graph.setDistanceScale(2.0f);
    graph.buildFullyConnectedGraph();
    REQUIRE_NEAR(graph.getDistance(0, 1), 10.0f, 0.001f);
    REQUIRE_NEAR(graph.getDistance(1, 0), 10.0f, 0.001f);
}

TEST_CASE(MapGraphInstallWeightedMatrixValidatesShape) {
    MapGraph graph = makeFourNodeGraph();
    REQUIRE_THROWS_AS(graph.installWeightedMatrix({{0.0f, 1.0f}}),
                      std::invalid_argument);
}

TEST_CASE(MapGraphGetNodeRejectsOutOfRangeIndex) {
    MapGraph graph = makeFourNodeGraph();
    REQUIRE_THROWS_AS(graph.getNode(-1), std::out_of_range);
    REQUIRE_THROWS_AS(graph.getNode(99), std::out_of_range);
}

TEST_CASE(MapGraphGetDistanceRejectsInvalidNodeId) {
    MapGraph graph = makeFourNodeGraph();
    REQUIRE_THROWS_AS(graph.getDistance(0, 99), std::invalid_argument);
}

TEST_CASE(MapGraphFindNodeIndexAndNeighborsWork) {
    MapGraph graph = makeFourNodeGraph();
    REQUIRE_EQ(graph.findNodeIndex(2), 2);
    REQUIRE_EQ(graph.findNodeIndex(42), -1);
    const auto neighbors = graph.getNeighbors(0);
    REQUIRE_EQ(neighbors.size(), static_cast<std::size_t>(3));
    REQUIRE_EQ(neighbors[0].first, 1);
}

TEST_CASE(MapGraphCalculateRouteDistanceUsesMatrixWeights) {
    MapGraph graph = makeFourNodeGraph();
    REQUIRE_NEAR(graph.calculateRouteDistance({0, 1, 2, 3, 0}),
                 16.0f, 0.001f);
}

TEST_CASE(MapGraphSetEdgeEventIsSymmetricAndClearable) {
    MapGraph graph = makeFourNodeGraph();
    graph.setEdgeEvent(1, 2, RoadEvent::FLOOD);
    REQUIRE_TRUE(graph.getEdgeEvent(1, 2) == RoadEvent::FLOOD);
    REQUIRE_TRUE(graph.getEdgeEvent(2, 1) == RoadEvent::FLOOD);
    REQUIRE_EQ(graph.getActiveEdgeEvents().size(), static_cast<std::size_t>(1));
    graph.clearAllEvents();
    REQUIRE_TRUE(graph.getEdgeEvent(1, 2) == RoadEvent::NONE);
}

TEST_CASE(MapGraphEffectiveDistanceAppliesBlockingPenalty) {
    MapGraph graph = makeFourNodeGraph();
    graph.setEdgeEvent(1, 2, RoadEvent::FESTIVAL);
    REQUIRE_NEAR(graph.getEffectiveDistance(1, 2),
                 3.0f * RoadEventRules::kImpassablePenalty, 0.1f);
}

TEST_CASE(MapGraphShortestPathAvoidsBlockedRoad) {
    MapGraph graph = makeFourNodeGraph();
    graph.setEdgeEvent(1, 2, RoadEvent::FLOOD);
    REQUIRE_NEAR(graph.getShortestPathDistance(1, 2), 5.0f, 0.001f);
}

TEST_CASE(MapGraphShortestPathFallsBackWhenNoUnblockedPathExists) {
    MapGraph graph;
    graph.addNode(WasteNode(0, "HQ", 0.0f, 0.0f, 0.0f, true));
    graph.addNode(WasteNode(1, "A", 1.0f, 0.0f, 100.0f));
    graph.buildFullyConnectedGraph();
    graph.installWeightedMatrix({{0.0f, 4.0f}, {4.0f, 0.0f}});
    graph.setEdgeEvent(0, 1, RoadEvent::FLOOD);
    REQUIRE_NEAR(graph.getShortestPathDistance(0, 1),
                 4.0f * RoadEventRules::kImpassablePenalty, 0.1f);
}

TEST_CASE(MapGraphThrowsWhenNoHqExists) {
    MapGraph graph;
    graph.addNode(WasteNode(1, "A", 0.0f, 0.0f, 100.0f));
    graph.buildFullyConnectedGraph();
    REQUIRE_THROWS_AS(graph.getHQNode(), std::runtime_error);
    REQUIRE_THROWS_AS(graph.getHQIndex(), std::runtime_error);
}

TEST_CASE(RegularRouteVisitsEligibleNodesInInputOrder) {
    const MapGraph graph = makeFourNodeGraph();
    const RouteResult result =
        runAlgorithm<RegularRouteAlgorithm>(graph, {3, 1, 2});
    requireRouteEquals(result, {0, 3, 1, 2, 0});
}

TEST_CASE(RegularRouteReturnsInvalidEmptyResultForNoEligibleNodes) {
    const MapGraph graph = makeFourNodeGraph();
    const RouteResult result = runAlgorithm<RegularRouteAlgorithm>(graph, {});
    REQUIRE_FALSE(result.isValid());
    REQUIRE_TRUE(result.visitOrder.empty());
}

TEST_CASE(GreedyRouteChoosesNearestCurrentNeighbor) {
    const MapGraph graph = makeTieFreeAlgorithmGraph();
    const RouteResult result = runAlgorithm<GreedyRouteAlgorithm>(graph);
    requireRouteEquals(result, {0, 1, 2, 3, 0});
}

TEST_CASE(GreedyRouteRespectsBlockedRoadWhenChoosingNextStop) {
    MapGraph graph = makeTieFreeAlgorithmGraph();
    graph.setEdgeEvent(1, 2, RoadEvent::FLOOD);
    graph.setEdgeEvent(0, 2, RoadEvent::FLOOD);
    const RouteResult result = runAlgorithm<GreedyRouteAlgorithm>(graph);
    requireRouteEquals(result, {0, 1, 3, 2, 0});
}

TEST_CASE(MstRouteProducesClosedCoverageRoute) {
    const MapGraph graph = makeTieFreeAlgorithmGraph();
    const RouteResult result = runAlgorithm<MSTRouteAlgorithm>(graph);
    requireValidClosedRoute(result, eligible123());
}

TEST_CASE(MstRouteIsDeterministicOnKnownGraph) {
    const MapGraph graph = makeTieFreeAlgorithmGraph();
    const RouteResult first = runAlgorithm<MSTRouteAlgorithm>(graph);
    const RouteResult second = runAlgorithm<MSTRouteAlgorithm>(graph);
    REQUIRE_TRUE(first.visitOrder == second.visitOrder);
    requireRouteEquals(first, {0, 1, 2, 3, 0});
}

TEST_CASE(TspRouteFindsExactOptimalSmallTour) {
    const MapGraph graph = makeTieFreeAlgorithmGraph();
    const RouteResult result = runAlgorithm<TSPRouteAlgorithm>(graph);
    requireValidClosedRoute(result, eligible123());
    REQUIRE_NEAR(routeDistance(graph, result), 14.0f, 0.001f);
}

TEST_CASE(TspRouteHandlesSingleEligibleNode) {
    const MapGraph graph = makeTieFreeAlgorithmGraph();
    const RouteResult result = runAlgorithm<TSPRouteAlgorithm>(graph, {2});
    requireRouteEquals(result, {0, 2, 0});
}

TEST_CASE(TspRouteFiltersHqFromEligibleInput) {
    const MapGraph graph = makeTieFreeAlgorithmGraph();
    const RouteResult result = runAlgorithm<TSPRouteAlgorithm>(graph, {0, 1});
    requireRouteEquals(result, {0, 1, 0});
}

TEST_CASE(DijkstraRouteOrdersByDepotShortestDistance) {
    const MapGraph graph = makeTieFreeAlgorithmGraph();
    const RouteResult result = runAlgorithm<DijkstraRouteAlgorithm>(graph);
    requireRouteEquals(result, {0, 1, 2, 3, 0});
}

TEST_CASE(DijkstraRouteDepotOrderChangesWhenRoadBlocked) {
    MapGraph graph = makeTieFreeAlgorithmGraph();
    graph.setEdgeEvent(0, 1, RoadEvent::FLOOD);
    graph.setEdgeEvent(1, 2, RoadEvent::FLOOD);
    const RouteResult result = runAlgorithm<DijkstraRouteAlgorithm>(graph);
    requireRouteEquals(result, {0, 2, 3, 1, 0});
}

TEST_CASE(BellmanFordRouteUsesCheapestInsertionAndCoversNodes) {
    const MapGraph graph = makeFiveNodeGraphForInsertionAlgorithms();
    const RouteResult result =
        runAlgorithm<BellmanFordRouteAlgorithm>(graph, {1, 2, 3, 4});
    requireValidClosedRoute(result, {1, 2, 3, 4});
    REQUIRE_NEAR(routeDistance(graph, result), 14.0f, 0.001f);
}

TEST_CASE(BellmanFordRouteHandlesOneNodeTour) {
    const MapGraph graph = makeTieFreeAlgorithmGraph();
    const RouteResult result =
        runAlgorithm<BellmanFordRouteAlgorithm>(graph, {3});
    requireRouteEquals(result, {0, 3, 0});
}

TEST_CASE(FloydWarshallRouteUsesFarthestInsertionAndCoversNodes) {
    const MapGraph graph = makeFiveNodeGraphForInsertionAlgorithms();
    const RouteResult result =
        runAlgorithm<FloydWarshallRouteAlgorithm>(graph, {1, 2, 3, 4});
    requireValidClosedRoute(result, {1, 2, 3, 4});
    REQUIRE_NEAR(routeDistance(graph, result), 14.0f, 0.001f);
}

TEST_CASE(FloydWarshallRouteHandlesOneNodeTour) {
    const MapGraph graph = makeTieFreeAlgorithmGraph();
    const RouteResult result =
        runAlgorithm<FloydWarshallRouteAlgorithm>(graph, {3});
    requireRouteEquals(result, {0, 3, 0});
}

TEST_CASE(AllAlgorithmsReturnValidClosedToursOnSameScenario) {
    const MapGraph graph = makeFiveNodeGraphForInsertionAlgorithms();
    const std::vector<int> eligible = {1, 2, 3, 4};

    requireValidClosedRoute(runAlgorithm<RegularRouteAlgorithm>(graph, eligible),
                            eligible);
    requireValidClosedRoute(runAlgorithm<GreedyRouteAlgorithm>(graph, eligible),
                            eligible);
    requireValidClosedRoute(runAlgorithm<MSTRouteAlgorithm>(graph, eligible),
                            eligible);
    requireValidClosedRoute(runAlgorithm<TSPRouteAlgorithm>(graph, eligible),
                            eligible);
    requireValidClosedRoute(runAlgorithm<DijkstraRouteAlgorithm>(graph, eligible),
                            eligible);
    requireValidClosedRoute(runAlgorithm<BellmanFordRouteAlgorithm>(graph, eligible),
                            eligible);
    requireValidClosedRoute(runAlgorithm<FloydWarshallRouteAlgorithm>(graph, eligible),
                            eligible);
}

TEST_CASE(AllAlgorithmsReturnEmptyForEmptyEligibleSet) {
    const MapGraph graph = makeFourNodeGraph();
    REQUIRE_FALSE(runAlgorithm<RegularRouteAlgorithm>(graph, {}).isValid());
    REQUIRE_FALSE(runAlgorithm<GreedyRouteAlgorithm>(graph, {}).isValid());
    REQUIRE_FALSE(runAlgorithm<MSTRouteAlgorithm>(graph, {}).isValid());
    REQUIRE_FALSE(runAlgorithm<TSPRouteAlgorithm>(graph, {}).isValid());
    REQUIRE_FALSE(runAlgorithm<DijkstraRouteAlgorithm>(graph, {}).isValid());
    REQUIRE_FALSE(runAlgorithm<BellmanFordRouteAlgorithm>(graph, {}).isValid());
    REQUIRE_FALSE(runAlgorithm<FloydWarshallRouteAlgorithm>(graph, {}).isValid());
}

TEST_CASE(AlgorithmResultsAreConsistentWithWasteSystemCostPopulation) {
    WasteSystem system;
    system.initializeMap();
    for (int i = 0; i < system.getGraph().getNodeCount(); ++i) {
        if (!system.getGraph().getNode(i).getIsHQ()) {
            system.getGraph().getNodeMutable(i).setWasteLevel(0.0f);
        }
    }
    system.getGraph().getNodeMutable(1).setWasteLevel(90.0f);
    system.getGraph().getNodeMutable(2).setWasteLevel(90.0f);

    GreedyRouteAlgorithm algorithm;
    RouteResult result = algorithm.computeRoute(system.getGraph(),
                                                system.getEligibleNodes(50.0f),
                                                0);
    system.populateCosts(result);

    REQUIRE_TRUE(result.isValid());
    REQUIRE_NEAR(result.totalDistance,
                 system.getGraph().calculateRouteDistance(result.visitOrder),
                 0.001f);
    REQUIRE_NEAR(result.totalCost,
                 result.fuelCost + result.wageCost + result.tollCost, 0.001f);
}

TEST_CASE(ComparisonManagerRegistersAllAlgorithms) {
    ComparisonManager manager;
    manager.initializeAlgorithms();
    REQUIRE_EQ(manager.getAlgorithmCount(), 7);
    REQUIRE_EQ(manager.getAlgorithmName(0), std::string("Regular Route"));
    REQUIRE_TRUE(manager.getAlgorithmDescription(3).find("bitmask") !=
                 std::string::npos);
    REQUIRE_EQ(manager.getAlgorithmName(100), std::string("Unknown"));
}

TEST_CASE(ComparisonManagerRejectsInvalidAlgorithmIndices) {
    WasteSystem system;
    system.initializeMap();
    ComparisonManager manager;
    manager.initializeAlgorithms();
    REQUIRE_THROWS_AS(manager.runSingleAlgorithm(-1, system), std::out_of_range);
    REQUIRE_THROWS_AS(manager.runSingleAlgorithm(7, system), std::out_of_range);
}

TEST_CASE(ComparisonManagerRunsSingleAlgorithmAndLogsCompletion) {
    WasteSystem system;
    system.initializeMap();
    system.getGraph().getNodeMutable(1).setWasteLevel(90.0f);
    ComparisonManager manager;
    manager.initializeAlgorithms();

    const int before = system.getEventLog().getCount();
    RouteResult result = manager.runSingleAlgorithm(1, system);
    system.populateCosts(result);

    REQUIRE_TRUE(result.isValid());
    REQUIRE_TRUE(system.getEventLog().getCount() > before);
}

TEST_CASE(ComparisonManagerRunsAllAlgorithmsAndSelectsLowestCost) {
    WasteSystem system;
    system.initializeMap();
    for (int i = 0; i < system.getGraph().getNodeCount(); ++i) {
        if (!system.getGraph().getNode(i).getIsHQ()) {
            system.getGraph().getNodeMutable(i).setWasteLevel(0.0f);
        }
    }
    system.getGraph().getNodeMutable(1).setWasteLevel(90.0f);
    system.getGraph().getNodeMutable(2).setWasteLevel(90.0f);
    system.getGraph().getNodeMutable(3).setWasteLevel(90.0f);

    ComparisonManager manager;
    manager.initializeAlgorithms();
    manager.runAllAlgorithms(system);

    REQUIRE_EQ(manager.getResults().size(), static_cast<std::size_t>(7));
    const int best = manager.getBestAlgorithmIndex();
    REQUIRE_TRUE(best >= 0);

    float bestCost = manager.getResults()[best].totalCost;
    for (const RouteResult& result : manager.getResults()) {
        REQUIRE_TRUE(result.isValid());
        REQUIRE_TRUE(bestCost <= result.totalCost + 0.001f);
    }
}

TEST_CASE(ComparisonManagerClearResultsEmptiesComparisonState) {
    WasteSystem system;
    system.initializeMap();
    system.getGraph().getNodeMutable(1).setWasteLevel(90.0f);
    ComparisonManager manager;
    manager.initializeAlgorithms();
    manager.runAllAlgorithms(system);
    REQUIRE_FALSE(manager.getResults().empty());
    manager.clearResults();
    REQUIRE_TRUE(manager.getResults().empty());
    REQUIRE_EQ(manager.getBestAlgorithmIndex(), -1);
}
