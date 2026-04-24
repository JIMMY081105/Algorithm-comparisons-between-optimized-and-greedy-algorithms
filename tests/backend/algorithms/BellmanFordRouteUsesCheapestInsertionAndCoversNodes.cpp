#include "GraphTestUtils.h"

TEST_CASE(BellmanFordRouteUsesCheapestInsertionAndCoversNodes) {
    const MapGraph graph = makeFiveNodeGraphForInsertionAlgorithms();
    const RouteResult result =
        runAlgorithm<BellmanFordRouteAlgorithm>(graph, {1, 2, 3, 4});
    requireValidClosedRoute(result, {1, 2, 3, 4});
    REQUIRE_NEAR(routeDistance(graph, result), 14.0f, 0.001f);
}
