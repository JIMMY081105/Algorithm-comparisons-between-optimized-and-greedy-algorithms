#include "GraphTestUtils.h"

TEST_CASE(ComparisonManagerRejectsInvalidAlgorithmIndices) {
    WasteSystem system;
    system.initializeMap();
    ComparisonManager manager;
    manager.initializeAlgorithms();
    REQUIRE_THROWS_AS(manager.runSingleAlgorithm(-1, system), std::out_of_range);
    REQUIRE_THROWS_AS(manager.runSingleAlgorithm(7, system), std::out_of_range);
}
