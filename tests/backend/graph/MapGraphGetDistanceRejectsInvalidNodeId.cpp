#include "GraphTestUtils.h"

TEST_CASE(MapGraphGetDistanceRejectsInvalidNodeId) {
    MapGraph graph = makeFourNodeGraph();
    REQUIRE_THROWS_AS(graph.getDistance(0, 99), std::invalid_argument);
}
