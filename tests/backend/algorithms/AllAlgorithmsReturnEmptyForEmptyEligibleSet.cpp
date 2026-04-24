#include "GraphTestUtils.h"

TEST_CASE(AllAlgorithmsReturnEmptyForEmptyEligibleSet) {
    const MapGraph graph = makeFourNodeGraph();
    REQUIRE_FALSE(runAlgorithm<RegularRouteAlgorithm>(graph, {}).isValid());
    REQUIRE_FALSE(runAlgorithm<GreedyRouteAlgorithm>(graph, {}).isValid());
    REQUIRE_FALSE(runAlgorithm<MSTRouteAlgorithm>(graph, {}).isValid());
    REQUIRE_FALSE(runAlgorithm<TSPRouteAlgorithm>(graph, {}).isValid());
    REQUIRE_FALSE(runAlgorithm<DijkstraRouteAlgorithm>(graph, {}).isValid());
    REQUIRE_FALSE(runAlgorithm<BellmanFordRouteAlgorithm>(graph, {}).isValid());
    REQUIRE_FALSE(runAlgorithm<FloydWarshallRouteAlgorithm>(graph, {}).isValid());
}
