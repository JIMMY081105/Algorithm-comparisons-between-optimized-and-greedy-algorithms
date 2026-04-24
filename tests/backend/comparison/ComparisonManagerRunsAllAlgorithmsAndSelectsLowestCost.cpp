#include "GraphTestUtils.h"

TEST_CASE(ComparisonManagerRunsAllAlgorithmsAndSelectsLowestCost) {
    WasteSystem system;
    system.initializeMap();
    for (int i = 0; i < system.getGraph().getNodeCount(); ++i) {
        if (!system.getGraph().getNode(i).getIsHQ()) {
            system.getGraph().getNodeMutable(i).setWasteLevel(0.0f);
        }
    }
    system.getGraph().getNodeMutable(1).setWasteLevel(90.0f);
    system.getGraph().getNodeMutable(2).setWasteLevel(90.0f);
    system.getGraph().getNodeMutable(3).setWasteLevel(90.0f);

    ComparisonManager manager;
    manager.initializeAlgorithms();
    manager.runAllAlgorithms(system);

    REQUIRE_EQ(manager.getResults().size(), static_cast<std::size_t>(7));
    const int best = manager.getBestAlgorithmIndex();
    REQUIRE_TRUE(best >= 0);

    float bestCost = manager.getResults()[best].totalCost;
    for (const RouteResult& result : manager.getResults()) {
        REQUIRE_TRUE(result.isValid());
        REQUIRE_TRUE(bestCost <= result.totalCost + 0.001f);
    }
}
