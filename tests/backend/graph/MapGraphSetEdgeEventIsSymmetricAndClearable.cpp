#include "GraphTestUtils.h"

TEST_CASE(MapGraphSetEdgeEventIsSymmetricAndClearable) {
    MapGraph graph = makeFourNodeGraph();
    graph.setEdgeEvent(1, 2, RoadEvent::FLOOD);
    REQUIRE_TRUE(graph.getEdgeEvent(1, 2) == RoadEvent::FLOOD);
    REQUIRE_TRUE(graph.getEdgeEvent(2, 1) == RoadEvent::FLOOD);
    REQUIRE_EQ(graph.getActiveEdgeEvents().size(), static_cast<std::size_t>(1));
    graph.clearAllEvents();
    REQUIRE_TRUE(graph.getEdgeEvent(1, 2) == RoadEvent::NONE);
}
