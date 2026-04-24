#include "GraphTestUtils.h"

TEST_CASE(MapGraphShortestPathFallsBackWhenNoUnblockedPathExists) {
    MapGraph graph;
    graph.addNode(WasteNode(0, "HQ", 0.0f, 0.0f, 0.0f, true));
    graph.addNode(WasteNode(1, "A", 1.0f, 0.0f, 100.0f));
    graph.buildFullyConnectedGraph();
    graph.installWeightedMatrix({{0.0f, 4.0f}, {4.0f, 0.0f}});
    graph.setEdgeEvent(0, 1, RoadEvent::FLOOD);
    REQUIRE_NEAR(graph.getShortestPathDistance(0, 1),
                 4.0f * RoadEventRules::kImpassablePenalty, 0.1f);
}
