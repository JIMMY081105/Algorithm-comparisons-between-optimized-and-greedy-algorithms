#include "GraphTestUtils.h"

TEST_CASE(MstRouteIsDeterministicOnKnownGraph) {
    const MapGraph graph = makeTieFreeAlgorithmGraph();
    const RouteResult first = runAlgorithm<MSTRouteAlgorithm>(graph);
    const RouteResult second = runAlgorithm<MSTRouteAlgorithm>(graph);
    REQUIRE_TRUE(first.visitOrder == second.visitOrder);
    requireRouteEquals(first, {0, 1, 2, 3, 0});
}
