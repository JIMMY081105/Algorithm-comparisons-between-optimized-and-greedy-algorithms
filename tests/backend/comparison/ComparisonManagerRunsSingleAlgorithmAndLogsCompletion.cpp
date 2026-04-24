#include "GraphTestUtils.h"

TEST_CASE(ComparisonManagerRunsSingleAlgorithmAndLogsCompletion) {
    WasteSystem system;
    system.initializeMap();
    system.getGraph().getNodeMutable(1).setWasteLevel(90.0f);
    ComparisonManager manager;
    manager.initializeAlgorithms();

    const int before = system.getEventLog().getCount();
    RouteResult result = manager.runSingleAlgorithm(1, system);
    system.populateCosts(result);

    REQUIRE_TRUE(result.isValid());
    REQUIRE_TRUE(system.getEventLog().getCount() > before);
}
