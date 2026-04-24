#include "GraphTestUtils.h"

TEST_CASE(FloydWarshallRouteHandlesOneNodeTour) {
    const MapGraph graph = makeTieFreeAlgorithmGraph();
    const RouteResult result =
        runAlgorithm<FloydWarshallRouteAlgorithm>(graph, {3});
    requireRouteEquals(result, {0, 3, 0});
}
