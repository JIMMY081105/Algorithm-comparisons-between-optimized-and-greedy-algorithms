#include "GraphTestUtils.h"

TEST_CASE(RegularRouteReturnsInvalidEmptyResultForNoEligibleNodes) {
    const MapGraph graph = makeFourNodeGraph();
    const RouteResult result = runAlgorithm<RegularRouteAlgorithm>(graph, {});
    REQUIRE_FALSE(result.isValid());
    REQUIRE_TRUE(result.visitOrder.empty());
}
