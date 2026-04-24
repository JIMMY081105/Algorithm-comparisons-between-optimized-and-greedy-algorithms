#include "GraphTestUtils.h"

TEST_CASE(MapGraphBuildsScaledEuclideanDistances) {
    MapGraph graph;
    graph.addNode(WasteNode(0, "HQ", 0.0f, 0.0f, 0.0f, true));
    graph.addNode(WasteNode(1, "A", 3.0f, 4.0f, 100.0f));
    graph.setDistanceScale(2.0f);
    graph.buildFullyConnectedGraph();
    REQUIRE_NEAR(graph.getDistance(0, 1), 10.0f, 0.001f);
    REQUIRE_NEAR(graph.getDistance(1, 0), 10.0f, 0.001f);
}
