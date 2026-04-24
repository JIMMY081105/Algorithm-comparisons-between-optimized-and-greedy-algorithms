#include "GraphTestUtils.h"

TEST_CASE(TspRouteFindsExactOptimalSmallTour) {
    const MapGraph graph = makeTieFreeAlgorithmGraph();
    const RouteResult result = runAlgorithm<TSPRouteAlgorithm>(graph);
    requireValidClosedRoute(result, eligible123());
    REQUIRE_NEAR(routeDistance(graph, result), 14.0f, 0.001f);
}
