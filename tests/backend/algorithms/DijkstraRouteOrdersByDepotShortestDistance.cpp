#include "GraphTestUtils.h"

TEST_CASE(DijkstraRouteOrdersByDepotShortestDistance) {
    const MapGraph graph = makeTieFreeAlgorithmGraph();
    const RouteResult result = runAlgorithm<DijkstraRouteAlgorithm>(graph);
    requireRouteEquals(result, {0, 1, 2, 3, 0});
}
