#include "GraphTestUtils.h"

TEST_CASE(MapGraphThrowsWhenNoHqExists) {
    MapGraph graph;
    graph.addNode(WasteNode(1, "A", 0.0f, 0.0f, 100.0f));
    graph.buildFullyConnectedGraph();
    REQUIRE_THROWS_AS(graph.getHQNode(), std::runtime_error);
    REQUIRE_THROWS_AS(graph.getHQIndex(), std::runtime_error);
}
