#include "GraphTestUtils.h"

TEST_CASE(GreedyRouteRespectsBlockedRoadWhenChoosingNextStop) {
    MapGraph graph = makeTieFreeAlgorithmGraph();
    graph.setEdgeEvent(1, 2, RoadEvent::FLOOD);
    graph.setEdgeEvent(0, 2, RoadEvent::FLOOD);
    const RouteResult result = runAlgorithm<GreedyRouteAlgorithm>(graph);
    requireRouteEquals(result, {0, 1, 3, 2, 0});
}
