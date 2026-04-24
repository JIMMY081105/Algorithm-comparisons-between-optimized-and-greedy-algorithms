#include "GraphTestUtils.h"

TEST_CASE(MapGraphShortestPathAvoidsBlockedRoad) {
    MapGraph graph = makeFourNodeGraph();
    graph.setEdgeEvent(1, 2, RoadEvent::FLOOD);
    REQUIRE_NEAR(graph.getShortestPathDistance(1, 2), 5.0f, 0.001f);
}
