#include "GraphTestUtils.h"

TEST_CASE(BellmanFordRouteHandlesOneNodeTour) {
    const MapGraph graph = makeTieFreeAlgorithmGraph();
    const RouteResult result =
        runAlgorithm<BellmanFordRouteAlgorithm>(graph, {3});
    requireRouteEquals(result, {0, 3, 0});
}
