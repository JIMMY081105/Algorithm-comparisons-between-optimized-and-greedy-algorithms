#include "GraphTestUtils.h"

TEST_CASE(TspRouteFiltersHqFromEligibleInput) {
    const MapGraph graph = makeTieFreeAlgorithmGraph();
    const RouteResult result = runAlgorithm<TSPRouteAlgorithm>(graph, {0, 1});
    requireRouteEquals(result, {0, 1, 0});
}
