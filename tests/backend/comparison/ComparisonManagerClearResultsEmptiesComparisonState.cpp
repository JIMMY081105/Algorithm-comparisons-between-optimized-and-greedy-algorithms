#include "GraphTestUtils.h"

TEST_CASE(ComparisonManagerClearResultsEmptiesComparisonState) {
    WasteSystem system;
    system.initializeMap();
    system.getGraph().getNodeMutable(1).setWasteLevel(90.0f);
    ComparisonManager manager;
    manager.initializeAlgorithms();
    manager.runAllAlgorithms(system);
    REQUIRE_FALSE(manager.getResults().empty());
    manager.clearResults();
    REQUIRE_TRUE(manager.getResults().empty());
    REQUIRE_EQ(manager.getBestAlgorithmIndex(), -1);
}
