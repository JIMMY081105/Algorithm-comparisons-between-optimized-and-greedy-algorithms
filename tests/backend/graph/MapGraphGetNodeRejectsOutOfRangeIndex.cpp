#include "GraphTestUtils.h"

TEST_CASE(MapGraphGetNodeRejectsOutOfRangeIndex) {
    MapGraph graph = makeFourNodeGraph();
    REQUIRE_THROWS_AS(graph.getNode(-1), std::out_of_range);
    REQUIRE_THROWS_AS(graph.getNode(99), std::out_of_range);
}
