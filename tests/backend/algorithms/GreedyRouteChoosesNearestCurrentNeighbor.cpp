#include "GraphTestUtils.h"

TEST_CASE(GreedyRouteChoosesNearestCurrentNeighbor) {
    const MapGraph graph = makeTieFreeAlgorithmGraph();
    const RouteResult result = runAlgorithm<GreedyRouteAlgorithm>(graph);
    requireRouteEquals(result, {0, 1, 2, 3, 0});
}
