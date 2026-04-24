#include "GraphTestUtils.h"

TEST_CASE(MapGraphFindNodeIndexAndNeighborsWork) {
    MapGraph graph = makeFourNodeGraph();
    REQUIRE_EQ(graph.findNodeIndex(2), 2);
    REQUIRE_EQ(graph.findNodeIndex(42), -1);
    const auto neighbors = graph.getNeighbors(0);
    REQUIRE_EQ(neighbors.size(), static_cast<std::size_t>(3));
    REQUIRE_EQ(neighbors[0].first, 1);
}
