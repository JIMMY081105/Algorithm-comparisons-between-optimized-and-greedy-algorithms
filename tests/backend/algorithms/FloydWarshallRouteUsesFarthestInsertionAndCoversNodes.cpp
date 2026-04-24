#include "GraphTestUtils.h"

TEST_CASE(FloydWarshallRouteUsesFarthestInsertionAndCoversNodes) {
    const MapGraph graph = makeFiveNodeGraphForInsertionAlgorithms();
    const RouteResult result =
        runAlgorithm<FloydWarshallRouteAlgorithm>(graph, {1, 2, 3, 4});
    requireValidClosedRoute(result, {1, 2, 3, 4});
    REQUIRE_NEAR(routeDistance(graph, result), 14.0f, 0.001f);
}
