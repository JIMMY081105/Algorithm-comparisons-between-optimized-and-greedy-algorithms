#include "GraphTestUtils.h"

TEST_CASE(MapGraphEffectiveDistanceAppliesBlockingPenalty) {
    MapGraph graph = makeFourNodeGraph();
    graph.setEdgeEvent(1, 2, RoadEvent::FESTIVAL);
    REQUIRE_NEAR(graph.getEffectiveDistance(1, 2),
                 3.0f * RoadEventRules::kImpassablePenalty, 0.1f);
}
