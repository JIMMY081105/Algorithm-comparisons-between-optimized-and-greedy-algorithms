#include "GraphTestUtils.h"

TEST_CASE(AlgorithmResultsAreConsistentWithWasteSystemCostPopulation) {
    WasteSystem system;
    system.initializeMap();
    for (int i = 0; i < system.getGraph().getNodeCount(); ++i) {
        if (!system.getGraph().getNode(i).getIsHQ()) {
            system.getGraph().getNodeMutable(i).setWasteLevel(0.0f);
        }
    }
    system.getGraph().getNodeMutable(1).setWasteLevel(90.0f);
    system.getGraph().getNodeMutable(2).setWasteLevel(90.0f);

    GreedyRouteAlgorithm algorithm;
    RouteResult result = algorithm.computeRoute(system.getGraph(),
                                                system.getEligibleNodes(50.0f),
                                                0);
    system.populateCosts(result);

    REQUIRE_TRUE(result.isValid());
    REQUIRE_NEAR(result.totalDistance,
                 system.getGraph().calculateRouteDistance(result.visitOrder),
                 0.001f);
    REQUIRE_NEAR(result.totalCost,
                 result.fuelCost + result.wageCost + result.tollCost, 0.001f);
}
