#include "GraphTestUtils.h"

TEST_CASE(TspRouteHandlesSingleEligibleNode) {
    const MapGraph graph = makeTieFreeAlgorithmGraph();
    const RouteResult result = runAlgorithm<TSPRouteAlgorithm>(graph, {2});
    requireRouteEquals(result, {0, 2, 0});
}
