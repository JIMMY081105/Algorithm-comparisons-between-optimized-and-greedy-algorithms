#include "GraphTestUtils.h"

TEST_CASE(RegularRouteVisitsEligibleNodesInInputOrder) {
    const MapGraph graph = makeFourNodeGraph();
    const RouteResult result =
        runAlgorithm<RegularRouteAlgorithm>(graph, {3, 1, 2});
    requireRouteEquals(result, {0, 3, 1, 2, 0});
}
