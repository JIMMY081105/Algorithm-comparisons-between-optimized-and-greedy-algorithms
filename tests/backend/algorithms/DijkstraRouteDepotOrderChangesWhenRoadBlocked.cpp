#include "GraphTestUtils.h"

TEST_CASE(DijkstraRouteDepotOrderChangesWhenRoadBlocked) {
    MapGraph graph = makeTieFreeAlgorithmGraph();
    graph.setEdgeEvent(0, 1, RoadEvent::FLOOD);
    graph.setEdgeEvent(1, 2, RoadEvent::FLOOD);
    const RouteResult result = runAlgorithm<DijkstraRouteAlgorithm>(graph);
    requireRouteEquals(result, {0, 2, 3, 1, 0});
}
