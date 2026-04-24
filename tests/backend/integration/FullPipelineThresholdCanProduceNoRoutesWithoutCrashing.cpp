#include "PersistenceTestUtils.h"

TEST_CASE(FullPipelineThresholdCanProduceNoRoutesWithoutCrashing) {
    WasteSystem system;
    system.initializeMap();
    system.generateNewDayWithSeed(9090u);
    system.setCollectionThreshold(101.0f);

    ComparisonManager manager;
    manager.initializeAlgorithms();
    manager.runAllAlgorithms(system);

    REQUIRE_EQ(manager.getResults().size(), static_cast<std::size_t>(7));
    REQUIRE_EQ(manager.getBestAlgorithmIndex(), -1);
    for (const RouteResult& result : manager.getResults()) {
        REQUIRE_FALSE(result.isValid());
        REQUIRE_TRUE(result.visitOrder.empty());
    }
}
