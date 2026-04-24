#ifndef GRAPH_TEST_UTILS_H
#define GRAPH_TEST_UTILS_H

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

inline MapGraph makeFourNodeGraph() {
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

inline MapGraph makeTieFreeAlgorithmGraph() {
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

inline MapGraph makeFiveNodeGraphForInsertionAlgorithms() {
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

inline std::vector<int> eligible123() {
    return {1, 2, 3};
}

template <typename Algorithm>
RouteResult runAlgorithm(const MapGraph& graph,
                         const std::vector<int>& eligible = eligible123()) {
    Algorithm algorithm;
    return algorithm.computeRoute(graph, eligible, 0);
}

inline void requireRouteEquals(const RouteResult& result,
                               const std::vector<int>& expected) {
    REQUIRE_TRUE(result.visitOrder == expected);
}

inline void requireValidClosedRoute(const RouteResult& result,
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

inline float routeDistance(const MapGraph& graph, const RouteResult& result) {
    return graph.calculateRouteDistance(result.visitOrder);
}

#endif // GRAPH_TEST_UTILS_H
