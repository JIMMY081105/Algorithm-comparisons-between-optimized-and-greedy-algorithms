#include "GraphTestUtils.h"

TEST_CASE(MstRouteProducesClosedCoverageRoute) {
    const MapGraph graph = makeTieFreeAlgorithmGraph();
    const RouteResult result = runAlgorithm<MSTRouteAlgorithm>(graph);
    requireValidClosedRoute(result, eligible123());
}
