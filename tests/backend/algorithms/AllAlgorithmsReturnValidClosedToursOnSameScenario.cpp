#include "GraphTestUtils.h"

TEST_CASE(AllAlgorithmsReturnValidClosedToursOnSameScenario) {
    const MapGraph graph = makeFiveNodeGraphForInsertionAlgorithms();
    const std::vector<int> eligible = {1, 2, 3, 4};

    requireValidClosedRoute(runAlgorithm<RegularRouteAlgorithm>(graph, eligible),
                            eligible);
    requireValidClosedRoute(runAlgorithm<GreedyRouteAlgorithm>(graph, eligible),
                            eligible);
    requireValidClosedRoute(runAlgorithm<MSTRouteAlgorithm>(graph, eligible),
                            eligible);
    requireValidClosedRoute(runAlgorithm<TSPRouteAlgorithm>(graph, eligible),
                            eligible);
    requireValidClosedRoute(runAlgorithm<DijkstraRouteAlgorithm>(graph, eligible),
                            eligible);
    requireValidClosedRoute(runAlgorithm<BellmanFordRouteAlgorithm>(graph, eligible),
                            eligible);
    requireValidClosedRoute(runAlgorithm<FloydWarshallRouteAlgorithm>(graph, eligible),
                            eligible);
}
